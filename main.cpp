#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <APRS-IS.h>
#include <APRS-Decoder.h>


//Definisco le funzioni del gateway

#define GATEWAY     true              //Invia i dati in rete sul server mqtt (Richiede internet)
#define REPEATER    false             //Ripete i dati che riceve tramite Lora in isofrequenza
#define QRZ         "IU7IGU-9"        //Nominativo da assegnare al GW

//Definisco i dati per la connessione WiFi

#define WIFI true
#define SSID "StellaRossa"
#define WIPSW "Marra123"

//Definisco i dati per il collegamento al server MQTT

#define MQTT_SERVER  "hamlorachat.freeddns.org"
const char* mqtt_user = "hamlorachat";
const char* mqtt_pass = "hamlorachat";
const char* topic = "/hamlorachat/Italia";
const char* topic_aprs = "/hamlorachat/aprs-it";

//Definisco l'interfaccia Lora

#define LORA_MOSI 27
#define LORA_SCLK 5
#define LORA_CS 18
#define LORA_DIO 26
#define LORA_ReST 23
#define LORA_MISO 19
#define LORA_BAND 433E6

#define LORA_POWER 20
#define LORA_FRQ 433775000
#define LORA_SPREAD 12
#define LORA_BANDW 125000

//Definisco l'interfaccia OLED

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

#define DEBUG     false

String inmex = "";
bool newmex = false;

bool mqtt = false;



unsigned long previousMillis = 0;
unsigned long interval = 30000;


Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire);

#if (GATEWAY)
  WiFiClient Wclient;

  PubSubClient client(Wclient);

#endif

String create_lat_aprs(double lat) {
  char str[20];
  char n_s = 'N';
  if (lat < 0) {
    n_s = 'S';
  }
  lat = std::abs(lat);
  sprintf(str, "%02d%05.2f%c", (int)lat, (lat - (double)((int)lat)) * 60.0, n_s);
  String lat_str(str);
  return lat_str;
}

String create_long_aprs(double lng) {
  char str[20];
  char e_w = 'E';
  if (lng < 0) {
    e_w = 'W';
  }
  lng = std::abs(lng);
  sprintf(str, "%03d%05.2f%c", (int)lng, (lng - (double)((int)lng)) * 60.0, e_w);
  String lng_str(str);
  return lng_str;
}


String splitta(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}


void callback(char *topic, byte *payload, unsigned int length) {
  newmex = true;
  String mex = "";
  for (int i=0;i<length;i++)
  {
    mex += (char)payload[i] ;
  }
  String rep = splitta(mex, '#', 5);        //Messaggio esempio chat/IU7IGU/lat/lon/testo/GW
  Serial.println(rep);
  if (rep != String(QRZ)){
  String qrz = splitta(mex, '#', 1);
  String message = splitta(mex, '#', 4);
  inmex = mex;
}}

void updatelcd(){

  display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(28,0);
  display.print("HamLoraChatGW");
  if (WiFi.status() == WL_CONNECTED){
  display.setCursor(5, 20);
  display.print("IP: ");
  display.print(WiFi.localIP());
  display.setCursor(5, 35);
  if (mqtt){
    display.println("MQTT: OK ");
  }
  else {
    display.println("MQTT: KO");
  }}
  display.display();
}



void setup(){
  if (DEBUG){
    Serial.begin(9600);
  }

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_ReST, LORA_DIO);
  LoRa.setSpreadingFactor(LORA_SPREAD);
  LoRa.setTxPower(LORA_POWER);
  LoRa.setSignalBandwidth(LORA_BANDW);
  LoRa.begin(LORA_FRQ);

  Wire.begin(OLED_SDA, OLED_SCL);

  updatelcd();

  if(GATEWAY){
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, WIPSW);
    if (DEBUG){
    Serial.print("Connecting to WiFi ..");
    }
    
    client.setServer(MQTT_SERVER, 1883);
    client.setCallback(callback);

    while (!client.connected()) {
    if (client.connect(QRZ, mqtt_user, mqtt_pass)) {
        if (DEBUG){
        Serial.println("Mqtt broker connected");
        }
        mqtt = true;
        updatelcd();
    } else {
      if (DEBUG){
        Serial.print("failed with state ");
      }
        mqtt = false;
        Serial.print(client.state());
        delay(2000);
        updatelcd();
    }
    }
    client.subscribe(topic);
    display.display();
  }
  if(REPEATER){
    updatelcd();
    display.setCursor(5, 20);
    display.print("REPEATER: OK");
    display.display();
  }
}

void loop(){
  int packetSize = LoRa.parsePacket();
  if(GATEWAY){
    client.loop();

    if(client.connected()){
      mqtt=true;
      updatelcd();
      delay(200);
    }else{
      mqtt=false;
      updatelcd();
      delay(200);
    }

   unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) {
    if (DEBUG){
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    }
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
}

  if (newmex and inmex != ""){
        LoRa.beginPacket();
        LoRa.print(inmex);
        if (DEBUG){
        Serial.println(inmex);
        }
        LoRa.endPacket(true);
        newmex = false;
        inmex = "";
  }

  if (packetSize) {
    // received a packet
      while (LoRa.available()) {
      String LoRaData = LoRa.readString();
      String tipo = splitta(LoRaData, '#', 0);
      if (splitta(LoRaData, '#', 5) == NULL){
      if (tipo == "chat"){
        String qrz = splitta(LoRaData, '#', 1);
        String lat = splitta(LoRaData, '#', 2);
        String lon = splitta (LoRaData, '#', 3);
        String message = splitta (LoRaData, '#', 4);
        //Serial.println(qrz);
        //Serial.println(message);
        if (GATEWAY){
        //String aprsmex = qrz+">APRS,TCPIP*,qAO,"+String(QRZ)+":="+create_lat_aprs(lat.toDouble())+"/"+create_long_aprs(lon.toDouble())+"-Talk on HamLoraChat - http://github.com/iu7igu/HamLoraChat";
        //aprsis.sendMessage(aprsmex);
        char mess[250];
        String messaggio = LoRaData + "#" + String(QRZ);
        messaggio.toCharArray(mess, 250);
        client.publish(topic, mess);
        break;
        }
      if (REPEATER){
          delay(500);
          LoRa.beginPacket();
          LoRa.print(LoRaData + "#" + String(QRZ));
          LoRa.endPacket(true);
      }
      }
      else if (tipo == "beacon"){
        String qrz = splitta(LoRaData, '#', 1);
        String lat = splitta(LoRaData, '#', 2);
        String lon = splitta (LoRaData, '#', 3);
        String beacon_message = splitta (LoRaData, '#', 4);
        //String aprsmex = qrz+">APRS,TCPIP*,qAO,"+String(QRZ)+":="+create_lat_aprs(lat.toDouble())+"/"+create_long_aprs(lon.toDouble())+beacon_message;
        //aprsis.sendMessage(aprsmex);
        char mess[250];
        LoRaData.toCharArray(mess, 250);
        client.publish(topic_aprs, mess);
      }
      }
    }
  }
}