/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
* This dictionnary holds the parameters sent to the network worker.
*/
dictionary NetworkCommandOptions
{
  long id = 0;                        // opaque id.
  DOMString cmd = "";                 // the command name.
  DOMString ifname;                   // for "removeNetworkRoute", "setDNS",
                                      //     "setDefaultRouteAndDNS", "removeDefaultRoute"
                                      //     "addHostRoute", "removeHostRoute"
                                      //     "removeHostRoutes".
  DOMString ip;                       // for "removeNetworkRoute", "setWifiTethering".
  unsigned long prefixLength;         // for "removeNetworkRoute".
  DOMString domain;                   // for "setDNS"
  sequence<DOMString> dnses;          // for "setDNS", "setDefaultRouteAndDNS".
  DOMString oldIfname;                // for "setDefaultRouteAndDNS".
  DOMString gateway;                  // for "addSecondaryRoute", "removeSecondaryRoute".
  sequence<DOMString> gateways;       // for "setDefaultRouteAndDNS", "removeDefaultRoute",
                                      //     "addHostRoute", "removeHostRoute".
  sequence<DOMString> hostnames;      // for "addHostRoute", "removeHostRoute".
  DOMString mode;                     // for "setWifiOperationMode".
  boolean report;                     // for "setWifiOperationMode".
  boolean isAsync;                    // for "setWifiOperationMode".
  boolean enabled;                    // for "setDhcpServer".
  DOMString wifictrlinterfacename;    // for "setWifiTethering".
  DOMString internalIfname;           // for "setWifiTethering".
  DOMString externalIfname;           // for "setWifiTethering".
  boolean enable;                     // for "setWifiTethering".
  DOMString ssid;                     // for "setWifiTethering".
  DOMString security;                 // for "setWifiTethering".
  DOMString key;                      // for "setWifiTethering".
  DOMString prefix;                   // for "setWifiTethering", "setDhcpServer".
  DOMString link;                     // for "setWifiTethering", "setDhcpServer".
  sequence<DOMString> interfaceList;  // for "setWifiTethering".
  DOMString wifiStartIp;              // for "setWifiTethering".
  DOMString wifiEndIp;                // for "setWifiTethering".
  DOMString usbStartIp;               // for "setWifiTethering".
  DOMString usbEndIp;                 // for "setWifiTethering".
  DOMString dns1;                     // for "setWifiTethering".
  DOMString dns2;                     // for "setWifiTethering".
  float rxBytes;                      // for "getNetworkInterfaceStats".
  float txBytes;                      // for "getNetworkInterfaceStats".
  DOMString date;                     // for "getNetworkInterfaceStats".
  long threshold;                     // for "setNetworkInterfaceAlarm",
                                      //     "enableNetworkInterfaceAlarm".
  DOMString startIp;                  // for "setDhcpServer".
  DOMString endIp;                    // for "setDhcpServer".
  DOMString serverIp;                 // for "setDhcpServer".
  DOMString maskLength;               // for "setDhcpServer".
  DOMString preInternalIfname;        // for "updateUpStream".
  DOMString preExternalIfname;        // for "updateUpStream".
  DOMString curInternalIfname;        // for "updateUpStream".
  DOMString curExternalIfname;        // for "updateUpStream".
};

/**
* This dictionary holds the parameters sent back to NetworkService.js.
*/
dictionary NetworkResultOptions
{
  long id = 0;                        // opaque id.
  boolean ret = false;                // for sync command.
  boolean broadcast = false;          // for netd broadcast message.
  DOMString topic = "";               // for netd broadcast message.
  DOMString reason = "";              // for netd broadcast message.

  long resultCode = 0;                // for all commands.
  DOMString resultReason = "";        // for all commands.
  boolean error = false;              // for all commands.

  float rxBytes = -1;                 // for "getNetworkInterfaceStats".
  float txBytes = -1;                 // for "getNetworkInterfaceStats".
  DOMString date = "";                // for "getNetworkInterfaceStats".
  boolean enable = false;             // for "setWifiTethering", "setUSBTethering"
                                      //     "enableUsbRndis".
  boolean result = false;             // for "enableUsbRndis".
  boolean success = false;            // for "setDhcpServer".
  DOMString curExternalIfname = "";   // for "updateUpStream".
  DOMString curInternalIfname = "";   // for "updateUpStream".
};
