//======================================================================
//  Program:Mesh gateway
//  Description: 
//  
//               
//               
//
//
//  
//                
//                
//                
//
//======================================================================
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <painlessMesh.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include "DHTesp.h"



#include "secrets.h"





int startupmessege;
long duration; 
const uint32_t JOB_PERIOD = 20000;  // 20 seconds

DHTesp dht;


// Prototypes
void receivedCallback( const uint32_t &from, const String &msg );
void mqttCallback(char* topic, byte* payload, unsigned int length);

IPAddress getlocalIP();

IPAddress myIP(10,10,50,40);


painlessMesh  mesh;
WiFiClient wifiClient;
PubSubClient mqttClient(mqttBroker, 1883, mqttCallback, wifiClient);








void receivedCallback( const uint32_t &from, const String &msg ) {
  Serial.printf("bridge: Received from %u msg=%s\n", from, msg.c_str());
  String topic = "iot/meshy/from/" + String(from);
  mqttClient.publish(topic.c_str(), msg.c_str());
}

void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  char* cleanPayload = (char*)malloc(length+1);
  memcpy(cleanPayload, payload, length);
  cleanPayload[length] = '\0';
  String msg = String(cleanPayload);
  free(cleanPayload);

  String targetStr = String(topic).substring(16);

  if(targetStr == "gateway")
  {
    if(msg == "getNodes")
    {
      auto nodes = mesh.getNodeList(true);
      String str;
      for (auto &&id : nodes)
        str += String(id) + String(" ");
      mqttClient.publish(mqtt_gateway_topic, str.c_str());
    }
  }
  else if(targetStr == "broadcast") 
  {
    mesh.sendBroadcast(msg);
  }
  else
  {
    uint32_t target = strtoul(targetStr.c_str(), NULL, 10);
    if(mesh.isConnected(target))
    {
      mesh.sendSingle(target, msg);
    }
    else
    {
      //mqttClient.publish(mqtt_debug_topic, "Client not connected!");
    }
  }
}

IPAddress getlocalIP() {
  return IPAddress(mesh.getStationIP());
}



void handle_status(String statusTopic, String statusMsg) {
  // Send to debug
  //handle_debug(true, statusMsg);
  
  // send to mqtt_status_topic
  if (mqttClient.connected())
  {


    char statusChar[50];
    statusMsg.toCharArray(statusChar,50);

    char statusChar1[50];
    statusTopic = mqtt_status_topic + statusTopic;
    statusTopic.toCharArray(statusChar1,50);
    

    mqttClient.publish(statusChar1, statusChar);
  }
}



void handle_tempsensor() {
  handle_status("/temperature", String(dht.getTemperature()));
  handle_status("/humidity", String(dht.getHumidity()));
}







void setup() {
  Serial.begin(115200);

  startupmessege = 1;
  dht.setup(2, DHTesp::DHT22); // Connect DHT sensor to GPIO 17
  // start ArduinoOTA
  ArduinoOTA.setHostname(devicename);

  ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();




  mesh.setDebugMsgTypes( ERROR | STARTUP | CONNECTION );  // set before init() so that you can see startup messages

  // Channel set to 6. Make sure to use the same channel for your mesh and for you other
  // network (STATION_SSID)
  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 6 );
  mesh.onReceive(&receivedCallback);

  mesh.stationManual(STATION_SSID, STATION_PASSWORD);
  mesh.setHostname(HOSTNAME);

  // if you want your node to accept OTA firmware, simply include this line
  // with whatever role you want your hardware to be. For instance, a
  // mesh network may have a thermometer, rain detector, and bridge. Each of
  // those may require different firmware, so different roles are preferrable.
  //
  // MAKE SURE YOUR UPLOADED OTA FIRMWARE INCLUDES OTA SUPPORT OR YOU WILL LOSE
  // THE ABILITY TO UPLOAD MORE FIRMWARE OVER OTA. YOU ALSO WANT TO MAKE SURE
  // THE ROLES ARE CORRECT
  //mesh.initOTAReceive("mqttbride");


  // Bridge node, should (in most cases) be a root node. See [the wiki](https://gitlab.com/painlessMesh/painlessMesh/wikis/Possible-challenges-in-mesh-formation) for some background
  mesh.setRoot(true);
  // This node and all other nodes should ideally know the mesh contains a root, so call this on all nodes
  mesh.setContainsRoot(true);
}

void loop() {
  mesh.update();
  mqttClient.loop();

  if(myIP != getlocalIP()){
    myIP = getlocalIP();
    Serial.println("My IP is " + myIP.toString());
    if (mqttClient.connect("meshymaster", mqtt_username, mqtt_password, mqtt_lwt_topic, 1, true, "Offline")) {
      mqttClient.publish(mqtt_lwt_topic, "Online", true);
      // Send Boot message
      
      if (startupmessege == 1) {
        mqttClient.publish(mqtt_boot_topic, "Started", true);
        startupmessege = 0;
      }
      mqttClient.subscribe("iot/meshy/from/#");
    } 
  }

  static uint32_t previousMillis;
  if (millis() - previousMillis >= JOB_PERIOD) {
      handle_tempsensor();
      previousMillis += JOB_PERIOD;
   }

   ArduinoOTA.handle();

}




