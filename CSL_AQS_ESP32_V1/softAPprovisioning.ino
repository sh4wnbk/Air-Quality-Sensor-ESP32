#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "CSL_AQS_ESP32_V1.h"

/**
* Starts a wifi access point with a unique name 'csl-xxxx' and starts a server.
* When client connects with a browser at specified ip (192.168.4.1), serves a 
* provisioning page to client with fields for ssid, passcode and gsid entry
* (gsid is the unique identifier for google script). Parses the response and decodes 
* the string for any %-encoded characters, and saves to memory
*/
void softAPprovision() {

  WiFiServer server(80);
  WiFiClient client;

  // Get MAC address and make a unique ssid with it
  char mac_ssid[16];
  snprintf(mac_ssid, 16, "csl-%02x%02x", WiFi.softAPmacAddress()[4], WiFi.softAPmacAddress()[5]);
  
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  // start softAP
  if (WiFi.softAP(mac_ssid)) {  // start AP with mac_ssid
    
    Serial.printf("Provisioning: connect to WiFi %s\n.Then open browser to ip: %s\n", mac_ssid, WiFi.softAPIP().toString().c_str());
    display.printf("Connect to WiFi \n\n%s\n", mac_ssid, WiFi.softAPIP().toString().c_str());
    display.display();
  } else {
    Serial.println("SoftAP start failed!");
  }

  while (!WiFi.softAPgetStationNum()) {
    Serial.print('.');
    if (provisionInfo.noWifi) {
      return;
    }
    delay(1000);
  }
  // someone connected so start server
  display.setCursor(0,0);
  display.clearDisplay();
  Serial.printf("\nWeb server begin\nOpen browser to address %s\n ", WiFi.softAPIP().toString().c_str());
  display.printf("\nWeb server begin\n\nOpen browser to \naddress \n\n%s\n ", WiFi.softAPIP().toString().c_str());
  display.display();
  server.begin();

  // loop till done
  while (true) {
    client = server.accept();  // listen and accept clients
    if (client) {
      Serial.println("----- new client");
      String cl = "";
      while (client.connected()) {
        if (client.available()) {  // anything to read from client?
          char c = client.read();
          Serial.write(c);
          if (c == '\n') {                        // is it eol?
            if (cl.length() == 0) {               // was previous line empty too?
              client.println("HTTP/1.1 200 OK");  // respond ok
              client.println("Content-type:text/html");
              client.println();
              client.print(buildProvisioningPage());  // send provisioning page
              client.println();
              break;
            } else {
              if (cl.startsWith("GET /get?")) {  // does the line have the provisioning  info?
                // parse the provisioning info
                String ssid = cl.substring(cl.indexOf("SSID=") + 5, cl.indexOf("&pass"));
                ssid = decodeUrl(ssid);
                String passcode = cl.substring(cl.indexOf("passcode=") + 9, cl.indexOf("&GSI"));
                passcode = decodeUrl(passcode);
                String gsid = cl.substring(cl.indexOf("GSID=") + 5, cl.indexOf(" HTTP"));
                gsid = decodeUrl(gsid);
                //Serial.printf("Saving to memory ssid: %s, passcode: %s, gsid: %s\n", ssid.c_str(), passcode.c_str(), gsid.c_str());

                // save provisioning info to memory
                strcpy(provisionInfo.ssid,ssid.c_str());
                strcpy(provisionInfo.passcode,passcode.c_str());
                strcpy(provisionInfo.gsid,gsid.c_str());
                provisionInfo.valid = true;
                provisionInfo.noWifi = false;
                storeProvisionInfo();

                client.stop();            // got our data so stop
                WiFi.softAPdisconnect();  // disconnect and turn off
                return;
              } else {  // no provisioning info so clear line
                cl = "";
              }
            }
          } else {            // not eol so keep going
            if (c != '\r') {  // disregard line returns
              cl += c;        // and add everything else to the line
            }
          }
        }             // end of 'if client.available'
      }               // 'while client connected'
      client.stop();  // client disconnected so stop
    }
  }
}

// Decodes %-encoded strings (it's a thing)
String decodeUrl(const String& encoded) {
  String decoded = "";
  for (int i = 0; i < encoded.length(); ++i) {
    if (encoded[i] == '%') {
      if (i + 2 < encoded.length()) {
        // Extract hex digits
        char hex1 = encoded[i + 1];
        char hex2 = encoded[i + 2];
        // Convert hex to decimal
        int decimalValue = 0;
        if (isdigit(hex1)) {  // is it 0..9?
          decimalValue = hex1 - '0';
        } else if (isxdigit(hex1)) {                // is it a..f or A..F?
          decimalValue = (hex1 | 0x20) - 'a' + 10;  // Convert uppercase to lowercase
        }
        if (isdigit(hex2)) {
          decimalValue = decimalValue * 16 + (hex2 - '0');
        } else if (isxdigit(hex2)) {
          decimalValue = decimalValue * 16 + ((hex2 | 0x20) - 'a' + 10);
        }
        // Append decoded char
        decoded += (char)decimalValue;
        i += 2;  // Skip the next two characters
      } else {
        // Invalid encoded string
        decoded += encoded[i];
      }
    } else {
      decoded += encoded[i];
    }
  }
  return decoded;
}


// Provisioning webpage showing avaliable networks -- by Fahmin Raman
String buildProvisioningPage() {
  int n = WiFi.scanNetworks();
  String page = "<!DOCTYPE HTML><html><head><title>Provision</title></head><body>";
  page += "<form action=\"/get\">";
  page += "SSID: <select name=\"SSID\">";
  for (int i = 0; i < n; i++) {
    page += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + "</option>";
  }
  page += "</select><br>";
  page += "Passcode: <input type=\"password\" name=\"passcode\"><br>";
  page += "GSID: <input type=\"text\" name=\"GSID\"><br>";
  page += "<input type=\"submit\" value=\"Submit\">";
  page += "</form></body></html>";
  return page;
}


void connectToWiFi() {
  // try to connect to wifi or continue without wifi
  Serial.printf("Trying to connect to WiFi: %s\n", provisionInfo.ssid);
  Serial.printf("To force provisioning press button A\n");
  Serial.printf("To continue without WiFi press button B\n");
  display.setCursor(0, 0);
  display.clearDisplay();
  display.printf("Connecting to WiFi: \n\n%s\n", provisionInfo.ssid);
  display.printf("Provisioning: bttn A\n");
  display.printf("No WiFi: bttn B\n");
  display.display();
  
  char mac_ssid[16];
  snprintf(mac_ssid, 16, "csl-%02x%02x", WiFi.softAPmacAddress()[4], WiFi.softAPmacAddress()[5]);

  while (WiFi.status() != WL_CONNECTED && !provisionInfo.noWifi) {
    delay(10000);  // wait 10 in case forced provisioning

    if (!provisionInfo.valid) { // someone pressed button A
      display.clearDisplay();
      display.setCursor(0,0);
      Serial.println("Going into provisioning mode");
      display.println("Provisioning mode\n");
      display.display();
      
      softAPprovision();
    }

    if (provisionInfo.noWifi) { // someone pressed button B
      Serial.println("\nContinuing without WiFi connection");
      display.println("No WiFi mode");
      display.display();
      break;
    }

    // connect to wifi0
    WiFi.mode(WIFI_STA);
    WiFi.begin(provisionInfo.ssid, provisionInfo.passcode);
    Serial.println("Connecting WiFi... ");
    display.setCursor(0,0);
    display.clearDisplay();
    display.println("Provision Successfull\n\nConnecting WiFi...");
    display.display();

    while (WiFi.status() != WL_CONNECTED && !provisionInfo.noWifi && provisionInfo.valid) {
      Serial.print(".");
      delay(400);
    }

    if (WiFi.status() == WL_CONNECTED) {
      delay(1000);
      Serial.printf("Connected to WiFi: %s\n", provisionInfo.ssid);
      display.printf("\nConnected to WiFi: \n\n%s", provisionInfo.ssid);
      display.display();
      break;
    }
  }
}
