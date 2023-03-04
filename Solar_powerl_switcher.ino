// ESP32 Dev Module COM16

#include <TaskScheduler.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <ArduinoOTA.h>
#include <ModbusIP_ESP8266.h>
#include "Wire.h"
#include "SHT31.h"

const char* ssid = "your SSID here";
const char* password = "your password here";

const char apid[] = "PowerController";  //  your network SSID (name)
const char appass[] = "your password here";       // your network password

IPAddress apIP(192, 168, 4, 1);

ModbusIP mb;

const int Coil_REG = 100;               // Modbus Offsets
const int Env_REG = 200;

unsigned int outputprog;
unsigned int lastprogress;

int wificounter = 0;

#define RELAY_PIN_1 21
#define RELAY_PIN_2 19
#define RELAY_PIN_3 18
#define RELAY_PIN_4 5
#define LED_PIN     25

#define SHT31_ADDRESS   0x44

#define SDA_pin 32
#define SCL_pin 33

SHT31 sht;


Scheduler runner;
void readenviros();

Task t1(1000, TASK_FOREVER, &readenviros);

void setup()
{
    pinMode(RELAY_PIN_1, OUTPUT);
    pinMode(RELAY_PIN_2, OUTPUT);
    pinMode(RELAY_PIN_3, OUTPUT);
    pinMode(RELAY_PIN_4, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.waitForConnectResult() != WL_CONNECTED && wificounter != 2) 
  {    
    delay(5000);
    ++wificounter;
  }

  if(WiFi.waitForConnectResult() != WL_CONNECTED)
  {    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apid, appass,5);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));  
    IPAddress myIP = WiFi.softAPIP();    
  }
  
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()      
      lastprogress = 0;
    })
    .onEnd([]() {
            
    })
    .onProgress([](unsigned int progress, unsigned int total) {      
      outputprog = progress / (total / 100);
      if (lastprogress != outputprog)
      {                        
        lastprogress = outputprog;
      }
    })
    .onError([](ota_error_t error) {      
      
    });

  ArduinoOTA.begin();

  Wire.begin(SDA_pin,SCL_pin);
  sht.begin(SHT31_ADDRESS, &Wire);
  Wire.setClock(100000);
 
  mb.server();  
  mb.addCoil(Coil_REG, 0, 4);
  mb.addHreg(Env_REG,0,2); 
   
  sht.requestData();
  delay(16);
  readenviros();

  runner.init();
  runner.addTask(t1);
  t1.enable();
}

void readenviros()
{
  bool success  = sht.readData();   // default = true = fast    
  if (success == false)
  {        
    mb.Hreg(Env_REG, 0);
    mb.Hreg((Env_REG + 1), 0); 
  }
  else
  {        
    mb.Hreg(Env_REG, sht.getRawTemperature());
    mb.Hreg((Env_REG + 1), sht.getRawHumidity());
  }    
  sht.requestData();                // request for next sample
}

void loop()
{ 
  ArduinoOTA.handle();  
  
  if (lastprogress == 0 && WiFi.status() == WL_CONNECTED)
  { 
    runner.execute();
    mb.task();
    
    digitalWrite(RELAY_PIN_1, mb.Coil(Coil_REG));
    digitalWrite(RELAY_PIN_2, mb.Coil(Coil_REG + 1));
    digitalWrite(RELAY_PIN_3, mb.Coil(Coil_REG + 2));
    digitalWrite(RELAY_PIN_4, mb.Coil(Coil_REG + 3));
    digitalWrite(LED_PIN, LOW); 
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);    
  }

}
