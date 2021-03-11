//
// ESP8266 Framework Program with Web-Server and
// WIFI Setup per Web-Front-End.
//
#include <Arduino.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WiFiMulti.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Utils.h"

// Global Definitions from Build System
//
#ifndef DEVICE_NAME
#define DEVICE_NAME "TestDevice"
#endif
#ifndef VERSION
#define VERSION Test
#endif
#define MAKE_STR(S) STR(S)
#define STR(S) #S

// Use from 0 to 4. Higher number, more debugging messages and memory usage.
#define _ESPASYNC_WIFIMGR_LOGLEVEL_    3

ESP8266WiFiMulti wifiMulti;

FS* filesystem = &LittleFS;

#define LED_ON LOW
#define LED_OFF HIGH


// For Config Portal
// SSID and PW for Config Portal
String ssid           = "ESP_" + String(ESP.getChipId(), HEX);
const char* password  = "your_password";

// SSID and PW for your Router
String Router_SSID;
String Router_Pass;

// You only need to format the filesystem once
//#define FORMAT_FILESYSTEM       true
#define FORMAT_FILESYSTEM         false

#define MIN_AP_PASSWORD_SIZE    8
#define SSID_MAX_LEN            32
#define PASS_MAX_LEN            64

typedef struct
{
  char wifi_ssid[SSID_MAX_LEN];
  char wifi_pw  [PASS_MAX_LEN];
}  WiFi_Credentials;

typedef struct
{
  String wifi_ssid;
  String wifi_pw;
}  WiFi_Credentials_String;

#define NUM_WIFI_CREDENTIALS      2

typedef struct
{
  WiFi_Credentials  WiFi_Creds [NUM_WIFI_CREDENTIALS];
} WM_Config;

WM_Config         WM_config;

#define  CONFIG_FILENAME              F("/wifi_cred.dat")
//////

// Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
bool initialConfig = false;

// SSID and PW for Config Portal
String AP_SSID;
String AP_PASS;

// Use false if you don't like to display Available Pages in Information Page of Config Portal
// Comment out or use true to display Available Pages in Information Page of Config Portal
// Must be placed before #include <ESPAsync_WiFiManager.h>
#define USE_AVAILABLE_PAGES     false

// From v1.0.10 to permit disable/enable StaticIP configuration in Config Portal from sketch. Valid only if DHCP is used.
// You'll loose the feature of dynamically changing from DHCP to static IP, or vice versa
// You have to explicitly specify false to disable the feature.
//#define USE_STATIC_IP_CONFIG_IN_CP          false

// Use false to disable NTP config. Advisable when using Cellphone, Tablet to access Config Portal.
// See Issue 23: On Android phone ConfigPortal is unresponsive (https://github.com/khoih-prog/ESP_WiFiManager/issues/23)
#define USE_ESP_WIFIMANAGER_NTP     false

// Use true to enable CloudFlare NTP service. System can hang if you don't have Internet access while accessing CloudFlare
// See Issue #21: CloudFlare link in the default portal (https://github.com/khoih-prog/ESP_WiFiManager/issues/21)
#define USE_CLOUDFLARE_NTP          false

// New in v1.0.11
#define USING_CORS_FEATURE          true
//////

// Use USE_DHCP_IP == true for dynamic DHCP IP, false to use static IP which you have to change accordingly to your network
#if (defined(USE_STATIC_IP_CONFIG_IN_CP) && !USE_STATIC_IP_CONFIG_IN_CP)
// Force DHCP to be true
#if defined(USE_DHCP_IP)
#undef USE_DHCP_IP
#endif
#define USE_DHCP_IP     true
#else
// You can select DHCP or Static IP here
//#define USE_DHCP_IP     true
#define USE_DHCP_IP     false
#endif

#if ( USE_DHCP_IP )
// Use DHCP
#warning Using DHCP IP
IPAddress stationIP   = IPAddress(0, 0, 0, 0);
IPAddress gatewayIP   = IPAddress(192, 168, 2, 1);
IPAddress netMask     = IPAddress(255, 255, 255, 0);
#else
// Use static IP
#warning Using static IP
#ifdef ESP32
IPAddress stationIP   = IPAddress(192, 168, 2, 232);
#else
IPAddress stationIP   = IPAddress(192, 168, 2, 186);
#endif

IPAddress gatewayIP   = IPAddress(192, 168, 2, 1);
IPAddress netMask     = IPAddress(255, 255, 255, 0);
#endif

#define USE_CONFIGURABLE_DNS      true

IPAddress dns1IP      = gatewayIP;
IPAddress dns2IP      = IPAddress(8, 8, 8, 8);

IPAddress APStaticIP  = IPAddress(192, 168, 100, 1);
IPAddress APStaticGW  = IPAddress(192, 168, 100, 1);
IPAddress APStaticSN  = IPAddress(255, 255, 255, 0);

#include <ESPAsync_WiFiManager.h>

#define HTTP_PORT           80

const char* CONFIG_FILE = "/config.json";

//define your default values here, if there are different values in config.json, they are overwritten.
//length should be max size + 1
char mqtt_server  [40]  = "mqtt-server.ddns.net";
char mqtt_port    [6]   = "1883";
char blynk_token  [33]  = "YOUR_BLYNK_TOKEN";

//flag for saving data
bool shouldSaveConfig = false;

///////////////////////////////////////////
// New in v1.4.0
/******************************************
 * // Defined in ESPAsync_WiFiManager.h
typedef struct
{
  IPAddress _ap_static_ip;
  IPAddress _ap_static_gw;
  IPAddress _ap_static_sn;
}  WiFi_AP_IPConfig;
typedef struct
{
  IPAddress _sta_static_ip;
  IPAddress _sta_static_gw;
  IPAddress _sta_static_sn;
#if USE_CONFIGURABLE_DNS
  IPAddress _sta_static_dns1;
  IPAddress _sta_static_dns2;
#endif
}  WiFi_STA_IPConfig;
******************************************/

WiFi_AP_IPConfig  WM_AP_IPconfig;
WiFi_STA_IPConfig WM_STA_IPconfig;

void initAPIPConfigStruct(WiFi_AP_IPConfig &in_WM_AP_IPconfig)
{
  in_WM_AP_IPconfig._ap_static_ip   = APStaticIP;
  in_WM_AP_IPconfig._ap_static_gw   = APStaticGW;
  in_WM_AP_IPconfig._ap_static_sn   = APStaticSN;
}

void initSTAIPConfigStruct(WiFi_STA_IPConfig &in_WM_STA_IPconfig)
{
  in_WM_STA_IPconfig._sta_static_ip   = stationIP;
  in_WM_STA_IPconfig._sta_static_gw   = gatewayIP;
  in_WM_STA_IPconfig._sta_static_sn   = netMask;
#if USE_CONFIGURABLE_DNS
  in_WM_STA_IPconfig._sta_static_dns1 = dns1IP;
  in_WM_STA_IPconfig._sta_static_dns2 = dns2IP;
#endif
}

void displayIPConfigStruct(WiFi_STA_IPConfig in_WM_STA_IPconfig)
{
  LOGERROR3(F("stationIP ="), in_WM_STA_IPconfig._sta_static_ip, ", gatewayIP =", in_WM_STA_IPconfig._sta_static_gw);
  LOGERROR1(F("netMask ="), in_WM_STA_IPconfig._sta_static_sn);
#if USE_CONFIGURABLE_DNS
  LOGERROR3(F("dns1IP ="), in_WM_STA_IPconfig._sta_static_dns1, ", dns2IP =", in_WM_STA_IPconfig._sta_static_dns2);
#endif
}

void configWiFi(WiFi_STA_IPConfig in_WM_STA_IPconfig)
{
  #if USE_CONFIGURABLE_DNS
    // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
    WiFi.config(in_WM_STA_IPconfig._sta_static_ip, in_WM_STA_IPconfig._sta_static_gw, in_WM_STA_IPconfig._sta_static_sn, in_WM_STA_IPconfig._sta_static_dns1, in_WM_STA_IPconfig._sta_static_dns2);  
  #else
    // Set static IP, Gateway, Subnetmask, Use auto DNS1 and DNS2.
    WiFi.config(in_WM_STA_IPconfig._sta_static_ip, in_WM_STA_IPconfig._sta_static_gw, in_WM_STA_IPconfig._sta_static_sn);
  #endif
}

///////////////////////////////////////////

uint8_t connectMultiWiFi()
{
#if ESP32
  // For ESP32, this better be 0 to shorten the connect time.
  // For ESP32-S2, must be > 500
  #if ( ARDUINO_ESP32S2_DEV || ARDUINO_FEATHERS2 || ARDUINO_PROS2 || ARDUINO_MICROS2 )
    #define WIFI_MULTI_1ST_CONNECT_WAITING_MS           500L
  #else
    #define WIFI_MULTI_1ST_CONNECT_WAITING_MS           0L
  #endif
#else
  // For ESP8266, this better be 2200 to enable connect the 1st time
  #define WIFI_MULTI_1ST_CONNECT_WAITING_MS             2200L
#endif

#define WIFI_MULTI_CONNECT_WAITING_MS                   100L
  
  uint8_t status;

  LOGERROR(F("ConnectMultiWiFi with :"));
  
  if ( (Router_SSID != "") && (Router_Pass != "") )
  {
    LOGERROR3(F("* Flash-stored Router_SSID = "), Router_SSID, F(", Router_Pass = "), Router_Pass );
  }

  for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
  {
    // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
    if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
    {
      LOGERROR3(F("* Additional SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
    }
  }
  
  LOGERROR(F("Connecting MultiWifi..."));

  WiFi.mode(WIFI_STA);

#if !USE_DHCP_IP
  // New in v1.4.0
  configWiFi(WM_STA_IPconfig);
  //////
#endif

  int i = 0;
  status = wifiMulti.run();
  delay(WIFI_MULTI_1ST_CONNECT_WAITING_MS);

  while ( ( i++ < 20 ) && ( status != WL_CONNECTED ) )
  {
    status = wifiMulti.run();

    if ( status == WL_CONNECTED )
      break;
    else
      delay(WIFI_MULTI_CONNECT_WAITING_MS);
  }

  if ( status == WL_CONNECTED )
  {
    LOGERROR1(F("WiFi connected after time: "), i);
    LOGERROR3(F("SSID:"), WiFi.SSID(), F(",RSSI="), WiFi.RSSI());
    LOGERROR3(F("Channel:"), WiFi.channel(), F(",IP address:"), WiFi.localIP() );
  }
  else
    LOGERROR(F("WiFi not connected"));

  return status;
}

//callback notifying us of the need to save config
void saveConfigCallback ()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void heartBeatPrint()
{
  static int num = 1;

  if (WiFi.status() == WL_CONNECTED)
    Serial.print(F("H"));        // H means connected to WiFi
  else
    Serial.print(F("F"));        // F means not connected to WiFi

  if (num == 80)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(F(" "));
  }
}

void toggleLED()
{
  //toggle state
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

void check_WiFi()
{
  if ( (WiFi.status() != WL_CONNECTED) )
  {
    Serial.println(F("\nWiFi lost. Call connectMultiWiFi in loop"));
    connectMultiWiFi();
  }
}

void check_status()
{
  static ulong checkstatus_timeout  = 0;
  static ulong LEDstatus_timeout    = 0;
  static ulong checkwifi_timeout    = 0;
  static ulong currentMillis;

#define HEARTBEAT_INTERVAL    10000L
#define LED_INTERVAL          2000L
#define WIFICHECK_INTERVAL    1000L

  currentMillis = millis();

  // Check WiFi every WIFICHECK_INTERVAL (1) seconds.
  if ((currentMillis > checkwifi_timeout) || (checkwifi_timeout == 0))
  {
    check_WiFi();
    checkwifi_timeout = currentMillis + WIFICHECK_INTERVAL;
  }

  if ((currentMillis > LEDstatus_timeout) || (LEDstatus_timeout == 0))
  {
    // Toggle LED at LED_INTERVAL = 2s
    toggleLED();
    LEDstatus_timeout = currentMillis + LED_INTERVAL;
  }

  // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
  if ((currentMillis > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = currentMillis + HEARTBEAT_INTERVAL;
  }
}

bool loadConfigData()
{
  File file = LittleFS.open(CONFIG_FILENAME, "r");
  LOGERROR(F("LoadWiFiCfgFile "));

  memset(&WM_config,       0, sizeof(WM_config));

  // New in v1.4.0
  memset(&WM_STA_IPconfig, 0, sizeof(WM_STA_IPconfig));
  //////

  if (file)
  {
    file.readBytes((char *) &WM_config,   sizeof(WM_config));

    // New in v1.4.0
    file.readBytes((char *) &WM_STA_IPconfig, sizeof(WM_STA_IPconfig));
    //////

    file.close();
    LOGERROR(F("OK"));

    // New in v1.4.0
    displayIPConfigStruct(WM_STA_IPconfig);
    //////

    return true;
  }
  else
  {
    LOGERROR(F("failed"));

    return false;
  }
}

void saveConfigData()
{
  File file = LittleFS.open(CONFIG_FILENAME, "w");
  LOGERROR(F("SaveWiFiCfgFile "));

  if (file)
  {
    file.write((uint8_t*) &WM_config,   sizeof(WM_config));

    displayIPConfigStruct(WM_STA_IPconfig);

    // New in v1.4.0
    file.write((uint8_t*) &WM_STA_IPconfig, sizeof(WM_STA_IPconfig));
    //////

    file.close();
    LOGERROR(F("OK"));
  }
  else
  {
    LOGERROR(F("failed"));
  }
}

void setup()
{
  //set led pin as output
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_ON);

  // Put your setup code here, to run once
  Serial.begin(115200);
  while (!Serial);

  //clean FS, for testing
  //FileFS.format();

  // Mount the filesystem
  bool result = LittleFS.begin();

#if USE_LITTLEFS
  Serial.print(F("\nLittleFS opened: "));
#else
  Serial.print(F("\nSPIFFS opened: "));
#endif

  Serial.println(result ? F("OK") : F("Failed"));

  //read configuration from FS json
  Serial.println(F("Mounting FS..."));

  if (result) 
  {
    Serial.println(F("Mounted file system"));

    if (LittleFS.exists(CONFIG_FILE)) 
    {
      //file exists, reading and loading
      Serial.println(F("Reading config file"));

      File configFile = LittleFS.open(CONFIG_FILE, "r");

      if (configFile) 
      {
        Serial.println(F("Opened config file"));

        size_t size = configFile.size();

        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

#if (ARDUINOJSON_VERSION_MAJOR >= 6)

        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());

        serializeJson(json, Serial);

        if ( !deserializeError )
#else
        DynamicJsonBuffer jsonBuffer;

        // Parse JSON string
        JsonObject& json = jsonBuffer.parseObject(buf.get());

        json.printTo(Serial);

        // Test if parsing succeeds.
        if (json.success())
#endif
        {
          Serial.println(F("\nParsed json"));

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port,   json["mqtt_port"]);
          strcpy(blynk_token, json["blynk_token"]);
        }
        else
        {
          Serial.println(F("Failed to load json config"));
        }
      }
    }
  }
  else
  {
    Serial.println(F("Failed to mount FS"));
  }

  //end read
  Serial.println(String("MQTT Server = ") + mqtt_server);
  Serial.println(String("MQTT Port   = ") + mqtt_port);
  Serial.println(String("Blynk Token = ") + blynk_token);

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  ESPAsync_WMParameter custom_mqtt_server ("server",  "mqtt server",  mqtt_server,  40);
  ESPAsync_WMParameter custom_mqtt_port   ("port",    "mqtt port",    mqtt_port,    5);
  ESPAsync_WMParameter custom_blynk_token ("blynk",   "blynk token",  blynk_token,  34);

  unsigned long startedAt = millis();

  // New in v1.4.0
  initAPIPConfigStruct(WM_AP_IPconfig);
  initSTAIPConfigStruct(WM_STA_IPconfig);
  //////

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  // Use this to default DHCP hostname to ESP8266-XXXXXX or ESP32-XXXXXX
  //ESPAsync_WiFiManager ESPAsync_wifiManager(&webServer, &dnsServer);
  // Use this to personalize DHCP hostname (RFC952 conformed)
  AsyncWebServer webServer(HTTP_PORT);

#if ( ARDUINO_ESP32S2_DEV || ARDUINO_FEATHERS2 || ARDUINO_PROS2 || ARDUINO_MICROS2 )
  ESPAsync_WiFiManager ESPAsync_wifiManager(&webServer, NULL, "AutoConnect-FSParams");
#else
  DNSServer dnsServer;

  ESPAsync_WiFiManager ESPAsync_wifiManager(&webServer, &dnsServer, "AutoConnect-FSParams");
#endif

  //set config save notify callback
  ESPAsync_wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  ESPAsync_wifiManager.addParameter(&custom_mqtt_server);
  ESPAsync_wifiManager.addParameter(&custom_mqtt_port);
  ESPAsync_wifiManager.addParameter(&custom_blynk_token);

  //reset settings - for testing
  //ESPAsync_wifiManager.resetSettings();

  ESPAsync_wifiManager.setDebugOutput(true);

  //set custom ip for portal
  // New in v1.4.0
  ESPAsync_wifiManager.setAPStaticIPConfig(WM_AP_IPconfig);
  //////

  //set minimum quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //ESPAsync_wifiManager.setMinimumSignalQuality();
  ESPAsync_wifiManager.setMinimumSignalQuality(-1);

  // From v1.0.10 only
  // Set config portal channel, default = 1. Use 0 => random channel from 1-13
  ESPAsync_wifiManager.setConfigPortalChannel(0);
  //////

#if !USE_DHCP_IP
    // Set (static IP, Gateway, Subnetmask, DNS1 and DNS2) or (IP, Gateway, Subnetmask). New in v1.0.5
    // New in v1.4.0
    ESPAsync_wifiManager.setSTAStaticIPConfig(WM_STA_IPconfig);
    //////
#endif

  // New from v1.1.1
#if USING_CORS_FEATURE
  ESPAsync_wifiManager.setCORSHeader("Your Access-Control-Allow-Origin");
#endif

  // We can't use WiFi.SSID() in ESP32 as it's only valid after connected.
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS/LittleFS for this purpose
  Router_SSID = ESPAsync_wifiManager.WiFi_SSID();
  Router_Pass = ESPAsync_wifiManager.WiFi_Pass();

  //Remove this line if you do not want to see WiFi password printed
  Serial.println("ESP Self-Stored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);

  bool configDataLoaded = false;

  // From v1.1.0, Don't permit NULL password
  if ( (Router_SSID != "") && (Router_Pass != "") )
  {
    LOGERROR3(F("* Add SSID = "), Router_SSID, F(", PW = "), Router_Pass);
    wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());

    ESPAsync_wifiManager.setConfigPortalTimeout(120); //If no access point name has been previously entered disable timeout.
    Serial.println(F("Got ESP Self-Stored Credentials. Timeout 120s for Config Portal"));
  }
  else if (loadConfigData())
  {
    configDataLoaded = true;

    ESPAsync_wifiManager.setConfigPortalTimeout(120); //If no access point name has been previously entered disable timeout.
    Serial.println(F("Got stored Credentials. Timeout 120s for Config Portal")); 
  }
  else
  {
    // Enter CP only if no stored SSID on flash and file 
    Serial.println(F("Open Config Portal without Timeout: No stored Credentials."));
    initialConfig = true;
  }

  String chipID = String(ESP.getChipId(), HEX);
  chipID.toUpperCase();

  // SSID and PW for Config Portal
  AP_SSID = "ESP_" + chipID + "_AutoConnectAP";
  AP_PASS = "MyESP_" + chipID;

  if (initialConfig)
  {
    Serial.println(F("We haven't got any access point credentials, so get them now"));

    // Starts an access point
    //if (!ESPAsync_wifiManager.startConfigPortal((const char *) ssid.c_str(), password))
    if ( !ESPAsync_wifiManager.startConfigPortal(AP_SSID.c_str(), AP_PASS.c_str()) )
      Serial.println(F("Not connected to WiFi but continuing anyway."));
    else
      Serial.println(F("WiFi connected...yeey :)"));

    // Stored  for later usage, from v1.1.0, but clear first
    memset(&WM_config, 0, sizeof(WM_config));

    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
    {
      String tempSSID = ESPAsync_wifiManager.getSSID(i);
      String tempPW   = ESPAsync_wifiManager.getPW(i);

      if (strlen(tempSSID.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1)
        strcpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str());
      else
        strncpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1);

      if (strlen(tempPW.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1)
        strcpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str());
      else
        strncpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1);

      // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
      if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
      {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }

    // New in v1.4.0
    ESPAsync_wifiManager.getSTAStaticIPConfig(WM_STA_IPconfig);
    //////

    saveConfigData();
  }
  else
  {
    wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());
  }

  startedAt = millis();

  if (!initialConfig)
  {
    // Load stored data, the addAP ready for MultiWiFi reconnection
    if (!configDataLoaded)
      loadConfigData();

    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
    {
      // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
      if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
      {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }

    if ( WiFi.status() != WL_CONNECTED ) 
    {
      Serial.println(F("ConnectMultiWiFi in setup"));

      connectMultiWiFi();
    }
  }

  Serial.print(F("After waiting "));
  Serial.print((float) (millis() - startedAt) / 1000L);
  Serial.print(F(" secs more in setup(), connection result is "));

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print(F("connected. Local IP: "));
    Serial.println(WiFi.localIP());
  }
  else
    Serial.println(ESPAsync_wifiManager.getStatus(WiFi.status()));

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) 
  {
    Serial.println(F("Saving config"));

#if (ARDUINOJSON_VERSION_MAJOR >= 6)
    DynamicJsonDocument json(1024);
#else
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
#endif

    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"]   = mqtt_port;
    json["blynk_token"] = blynk_token;

    File configFile     = LittleFS.open(CONFIG_FILE, "w");

    if (!configFile) 
    {
      Serial.println(F("Failed to open config file for writing"));
    }

#if (ARDUINOJSON_VERSION_MAJOR >= 6)
    serializeJsonPretty(json, Serial);
    // Write data to file and close it
    serializeJson(json, configFile);
#else
    json.prettyPrintTo(Serial);
    // Write data to file and close it
    json.printTo(configFile);
#endif

    configFile.close();
    //end save
  }

  Serial.print(F("Local IP = "));
  Serial.println(WiFi.localIP());
  Serial.print(F("GateWay IP = "));
  Serial.println(WiFi.gatewayIP());
  Serial.print(F("SubnetMask = ")); 
  Serial.println(WiFi.subnetMask());

  //keep LED off
  digitalWrite(LED_BUILTIN, LED_OFF);
}

void setup2()
{
    Serial.begin(115200);
    delay(3000);
    Serial.println();
    Serial.println(StringPad("*", 50, '*'));
    Serial.println("*" + StringPad("*", -49, ' '));
    Serial.println("*  " + StringPad(DEVICE_NAME, 44, ' ')+"  *");
    Serial.println("*" + StringPad("*", -49, ' '));
    Serial.println("*  Version: " + StringPad(MAKE_STR(VERSION), 35, ' ') + "  *");
    Serial.println("*" + StringPad("*", -49, ' '));
    Serial.println(StringPad("*", 50, '*'));
    Serial.println();
    Serial.println("Start");

    LittleFS.begin();
    Dir D = LittleFS.openDir("/");
    while (D.next())
    {
        Serial.println(D.fileName());
        Serial.print("-");
        Serial.println(StringPad(D.fileName(), 39, '-'));
        File F = D.openFile("r");
        int c = 0;
        while (c >= 0)
        {
            c = F.read();
            if (c >= 0)
            {
                Serial.print((char)c);
            }
        }
        Serial.println();
        Serial.println(F("----------------------------------------"));
    }

    pinMode(LED_BUILTIN, OUTPUT);
}

void ShowLED(int Pin, int Delay, bool On)
{
    if (On)
    {
        digitalWrite(Pin, LOW);
        Serial.println("The LED is on");
    }
    else
    {
        digitalWrite(Pin, HIGH);
        Serial.println("The LED is off");
    }
    delay(Delay);
}

void loop()
{
    check_status();
    ShowLED(LED_BUILTIN, 100, true);
    ShowLED(LED_BUILTIN, 100, false);
}