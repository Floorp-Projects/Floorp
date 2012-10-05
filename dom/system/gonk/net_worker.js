/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

const DEBUG = false;

const USB_FUNCTION_PATH  = "sys.usb.config";
const USB_FUNCTION_STATE = "sys.usb.state";
const USB_FUNCTION_RNDIS = "rndis,adb";
const USB_FUNCTION_ADB   = "adb";
// Retry 20 times (2 seconds) for usb state transition.
const USB_FUNCTION_RETRY_TIMES = 20;
// Check "sys.usb.state" every 100ms.
const USB_FUNCTION_RETRY_INTERVAL = 100;

// 1xx - Requested action is proceeding
const NETD_COMMAND_PROCEEDING   = 100;
// 2xx - Requested action has been successfully completed
const NETD_COMMAND_OKAY         = 200;
// 4xx - The command is accepted but the requested action didn't
// take place.
const NETD_COMMAND_FAIL         = 400;
// 5xx - The command syntax or parameters error
const NETD_COMMAND_ERROR        = 500;
// 6xx - Unsolicited broadcasts
const NETD_COMMAND_UNSOLICITED  = 600;

importScripts("systemlibs.js");

function netdResponseType(code) {
  return Math.floor(code/100)*100;
}

function isError(code) {
  let type = netdResponseType(code);
  return (type != NETD_COMMAND_PROCEEDING && type != NETD_COMMAND_OKAY);
}

function isComplete(code) {
  let type = netdResponseType(code);
  return (type != NETD_COMMAND_PROCEEDING);
}

let gWifiFailChain = [stopSoftAP,
                      setIpForwardingEnabled,
                      stopTethering];

function wifiTetheringFail(params) {
  // Notify the main thread.
  postMessage(params);

  // If one of the stages fails, we try roll back to ensure
  // we don't leave the network systems in limbo.

  // This parameter is used to disable ipforwarding.
  params.enable = false;
  chain(params, gWifiFailChain, null);
}

function wifiTetheringSuccess(params) {
  // Notify the main thread.
  postMessage(params);
  return true;
}

let gUSBFailChain = [stopSoftAP,
                     setIpForwardingEnabled,
                     stopTethering];

function usbTetheringFail(params) {
  // Notify the main thread.
  postMessage(params);
  // Try to roll back to ensure
  // we don't leave the network systems in limbo.
  let functionChain = [setIpForwardingEnabled,
                       stopTethering];

  // This parameter is used to disable ipforwarding.
  params.enable = false;
  chain(params, gUSBFailChain, null);

  // Disable usb rndis function.
  setUSBFunction({usbfunc: USB_FUNCTION_ADB, report: false});
}

function usbTetheringSuccess(params) {
  // Notify the main thread.
  postMessage(params);
  return true;
}

function networkInterfaceStatsFail(params) {
  // Notify the main thread.
  postMessage(params);
  return true;
}

function networkInterfaceStatsSuccess(params) {
  // Notify the main thread.
  params.txBytes = parseFloat(params.resultReason);

  postMessage(params);
  return true;
}

/**
 * Get network interface properties from the system property table.
 *
 * @param ifname
 *        Name of the network interface.
 */
function getIFProperties(ifname) {
  let gateway_str = libcutils.property_get("net." + ifname + ".gw");
  return {
    ifname:      ifname,
    gateway:     netHelpers.stringToIP(gateway_str),
    gateway_str: gateway_str,
    dns1_str:    libcutils.property_get("net." + ifname + ".dns1"),
    dns2_str:    libcutils.property_get("net." + ifname + ".dns2"),
  };
}

/**
 * Routines accessible to the main thread.
 */

/**
 * Dispatch a message from the main thread to a function.
 */
self.onmessage = function onmessage(event) {
  let message = event.data;
  if (DEBUG) debug("received message: " + JSON.stringify(message));
  // We have to keep the id in message. It will be used when post the result
  // to NetworkManager later.
  let ret = self[message.cmd](message);
  if (!message.isAsync) {
    postMessage({id: message.id, ret: ret});
  }
};

/**
 * Set default route and DNS servers for given network interface.
 */
function setDefaultRouteAndDNS(options) {
  if (options.oldIfname) {
    libnetutils.ifc_remove_default_route(options.oldIfname);
  }

  if (!options.gateway || !options.dns1_str) {
    options = getIFProperties(options.ifname);
  }

  libnetutils.ifc_set_default_route(options.ifname, options.gateway);
  libcutils.property_set("net.dns1", options.dns1_str);
  libcutils.property_set("net.dns2", options.dns2_str);

  // Bump the DNS change property.
  let dnschange = libcutils.property_get("net.dnschange", "0");
  libcutils.property_set("net.dnschange", (parseInt(dnschange, 10) + 1).toString());
}

/**
 * Run DHCP and set default route and DNS servers for a given
 * network interface.
 */
function runDHCPAndSetDefaultRouteAndDNS(options) {
  let dhcp = libnetutils.dhcp_do_request(options.ifname);
  dhcp.ifname = options.ifname;
  dhcp.oldIfname = options.oldIfname;

  //TODO this could be race-y... by the time we've finished the DHCP request
  // and are now fudging with the routes, another network interface may have
  // come online that's preferred...
  setDefaultRouteAndDNS(dhcp);
}

/**
 * Remove default route for given network interface.
 */
function removeDefaultRoute(options) {
  libnetutils.ifc_remove_default_route(options.ifname);
}

/**
 * Add host route for given network interface.
 */
function addHostRoute(options) {
  libnetutils.ifc_add_route(options.ifname, options.dns1, 32, options.gateway);
  libnetutils.ifc_add_route(options.ifname, options.dns2, 32, options.gateway);
  libnetutils.ifc_add_route(options.ifname, options.httpproxy, 32, options.gateway);
  libnetutils.ifc_add_route(options.ifname, options.mmsproxy, 32, options.gateway);
}

/**
 * Remove host route for given network interface.
 */
function removeHostRoute(options) {
  libnetutils.ifc_remove_route(options.ifname, options.dns1, 32, options.gateway);
  libnetutils.ifc_remove_route(options.ifname, options.dns2, 32, options.gateway);
  libnetutils.ifc_remove_route(options.ifname, options.httpproxy, 32, options.gateway);
  libnetutils.ifc_remove_route(options.ifname, options.mmsproxy, 32, options.gateway);
}

let gCommandQueue = [];
let gCurrentCommand = null;
let gCurrentCallback = null;
let gPending = false;

/**
 * Handle received data from netd.
 */
function onNetdMessage(data) {
  let result = "";
  let reason = "";

  // The return result is separated from the reason by a space character.
  let i = 0;
  while (i < data.length) {
    let octet = data[i];
    i += 1;
    if (octet == 32) {
      break;
    }
    result += String.fromCharCode(octet);
  }

  let code = parseInt(result);

  for (; i < data.length; i++) {
    let octet = data[i];
    reason += String.fromCharCode(octet);
  }

  // Set pending to false before we handle next command.
  debug("Receiving '" + gCurrentCommand + "' command response from netd.");
  debug("          ==> Code: " + code + "  Reason: " + reason);

  // 1xx response code regards as command is proceeding, we need to wait for
  // final response code such as 2xx, 4xx and 5xx before sending next command.
  if (isComplete(code)) {
    gPending = false;
  }
  if (gCurrentCallback) {
    gCurrentCallback(isError(code), {code: code, reason: reason});
  }

  // Handling pending commands if any.
  if (isComplete(code)) {
    nextNetdCommand();
  }
}

/**
 * Send command to netd.
 */
function doCommand(command, callback) {
  debug("Preparing to send '" + command + "' command...");
  gCommandQueue.push([command, callback]);
  return nextNetdCommand();
}

function nextNetdCommand() {
  if (!gCommandQueue.length || gPending) {
    return true;
  }
  [gCurrentCommand, gCurrentCallback] = gCommandQueue.shift();
  debug("Sending '" + gCurrentCommand + "' command to netd.");
  gPending = true;
  return postNetdCommand(gCurrentCommand);
}

function setInterfaceUp(params, callback) {
  let command = "interface setcfg " + params.ifname + " " + params.ip + " " +
                params.prefix + " " + "[" + params.link + "]";
  return doCommand(command, callback);
}

function setInterfaceDown(params, callback) {
  let command = "interface setcfg " + params.ifname + " " + params.ip + " " +
                params.prefix + " " + "[" + params.link + "]";
  return doCommand(command, callback);
}

function setIpForwardingEnabled(params, callback) {
  let command;

  if (params.enable) {
    command = "ipfwd enable";
  } else {
    command = "ipfwd disable";
  }
  return doCommand(command, callback);
}

function startTethering(params, callback) {
  let command = "tether start " + params.startIp + " " + params.endIp;
  return doCommand(command, callback);
}

function stopTethering(params, callback) {
  let command = "tether stop";
  return doCommand(command, callback);
}

function tetherInterface(params, callback) {
  let command = "tether interface add " + params.ifname;
  return doCommand(command, callback);
}

function untetherInterface(params, callback) {
  let command = "tether interface remove " + params.ifname;
  return doCommand(command, callback);
}

function setDnsForwarders(params, callback) {
  let command = "tether dns set " + params.dns1 + " " + params.dns2;
  return doCommand(command, callback);
}

function enableNat(params, callback) {
  let command = "nat enable " + params.internalIfname + " " +
                params.externalIfname + " " + "0";
  return doCommand(command, callback);
}

function disableNat(params, callback) {
  let command = "nat disable " + params.internalIfname + " " +
                params.externalIfname + " " + "0";
  return doCommand(command, callback);
}

function wifiFirmwareReload(params, callback) {
  let command = "softap fwreload " + params.ifname + " " + params.mode;
  return doCommand(command, callback);
}

function startAccessPointDriver(params, callback) {
  let command = "softap start " + params.ifname;
  return doCommand(command, callback);
}

function stopAccessPointDriver(params, callback) {
  let command = "softap stop " + params.ifname;
  return doCommand(command, callback);
}

function startSoftAP(params, callback) {
  let command = "softap startap";
  return doCommand(command, callback);
}

function stopSoftAP(params, callback) {
  let command = "softap stopap";
  return doCommand(command, callback);
}

function getRxBytes(params, callback) {
  let command = "interface readrxcounter " + params.ifname;
  return doCommand(command, callback);
}

function getTxBytes(params, callback) {
  params.rxBytes = parseFloat(params.resultReason);

  let command = "interface readtxcounter " + params.ifname;
  return doCommand(command, callback);
}

// The command format is "softap set wlan0 wl0.1 hotspot456 open null 6 0 8".
function setAccessPoint(params, callback) {
  let command = "softap set " + params.ifname +
                " " + params.wifictrlinterfacename +
                " " + params.ssid +
                " " + params.security +
                " " + params.key +
                " " + "6 0 8";
  return doCommand(command, callback);
}

/**
 * Modify usb function's property to turn on USB RNDIS function
 */
function setUSBFunction(params) {
  let report = params.report;
  let retry = 0;
  let i = 0;

  libcutils.property_set(USB_FUNCTION_PATH, params.usbfunc);
  // Trigger the timer to check usb state and report the result to NetworkManager.
  if (report) {
    setTimeout(checkUSBFunction, USB_FUNCTION_RETRY_INTERVAL, params);
  }

  function checkUSBFunction(params) {
    let result = libcutils.property_get(USB_FUNCTION_STATE);
    if (result == params.usbfunc) {
      params.result = true;
      postMessage(params);
      retry = 0;
      return;
    }
    if (retry < USB_FUNCTION_RETRY_TIMES) {
      retry++;
      setTimeout(checkUSBFunction, USB_FUNCTION_RETRY_INTERVAL, params);
      return;
    }
    params.result = false;
    postMessage(params);
  };

  return true;
}

function dumpParams(params, type) {
  if (!DEBUG) {
    return;
  }

  debug("Dump params:");
  debug("     ifname: " + params.ifname);
  debug("     ip: " + params.ip);
  debug("     link: " + params.link);
  debug("     prefix: " + params.prefix);
  debug("     startIp: " + params.startIp);
  debug("     endIp: " + params.endIp);
  debug("     dnsserver1: " + params.dns1);
  debug("     dnsserver2: " + params.dns2);
  debug("     internalIfname: " + params.internalIfname);
  debug("     externalIfname: " + params.externalIfname);
  if (type == "WIFI") {
    debug("     wifictrlinterfacename: " + params.wifictrlinterfacename);
    debug("     ssid: " + params.ssid);
    debug("     security: " + params.security);
    debug("     key: " + params.key);
  }
}

function chain(params, funcs, onError) {
  let i = -1;
  let f = funcs[i];
  function next(error, result) {
    params.resultCode = result.code;
    params.resultReason = result.reason;
    if (error) {
      if (onError) {
        params.error = error;
        params.state = f.name;
        onError(params);
      }
      return;
    }
    i += 1;
    f = funcs[i];
    if (f) {
      let ret = f(params, next);
      if (!ret && onError) {
        params.error = true;
        params.state = f.name;
        onError(params);
      }
    }
  };
  next(null, {code: NETD_COMMAND_ERROR, reason: ""});
}

let gWifiEnableChain = [wifiFirmwareReload,
                        startAccessPointDriver,
                        setAccessPoint,
                        startSoftAP,
                        setInterfaceUp,
                        tetherInterface,
                        setIpForwardingEnabled,
                        startTethering,
                        setDnsForwarders,
                        enableNat,
                        wifiTetheringSuccess];

let gWifiDisableChain = [stopSoftAP,
                         stopAccessPointDriver,
                         wifiFirmwareReload,
                         disableNat,
                         untetherInterface,
                         setIpForwardingEnabled,
                         stopTethering,
                         wifiTetheringSuccess];

/**
 * handling main thread's enable/disable WiFi Tethering request
 */
function setWifiTethering(params) {
  let enable = params.enable;
  let interfaceProperties = getIFProperties(params.externalIfname);

  params.dns1 = interfaceProperties.dns1_str;
  params.dns2 = interfaceProperties.dns2_str;
  dumpParams(params, "WIFI");

  if (enable) {
    // Enable Wifi tethering.
    debug("Starting Wifi Tethering on " +
           params.internalIfname + "<->" + params.externalIfname);
    chain(params, gWifiEnableChain, wifiTetheringFail);
  } else {
    // Disable Wifi tethering.
    debug("Stopping Wifi Tethering on " +
           params.internalIfname + "<->" + params.externalIfname);
    chain(params, gWifiDisableChain, wifiTetheringFail);
  }
  return true;
}

let gUSBEnableChain = [setInterfaceUp,
                       enableNat,
                       setIpForwardingEnabled,
                       tetherInterface,
                       startTethering,
                       setDnsForwarders,
                       usbTetheringSuccess];

let gUSBDisableChain = [disableNat,
                        setIpForwardingEnabled,
                        stopTethering,
                        untetherInterface,
                        usbTetheringSuccess];

/**
 * handling main thread's enable/disable USB Tethering request
 */
function setUSBTethering(params) {
  let enable = params.enable;
  let interfaceProperties = getIFProperties(params.externalIfname);

  params.dns1 = interfaceProperties.dns1_str;
  params.dns2 = interfaceProperties.dns2_str;
  dumpParams(params, "USB");

  if (enable) {
    // Enable USB tethering.
    debug("Starting USB Tethering on " +
           params.internalIfname + "<->" + params.externalIfname);
    chain(params, gUSBEnableChain, usbTetheringFail);
  } else {
    // Disable USB tetehring.
    debug("Stopping USB Tethering on " +
           params.internalIfname + "<->" + params.externalIfname);
    chain(params, gUSBDisableChain, usbTetheringFail);
  }
  return true;
}

let gNetworkInterfaceStatsChain = [getRxBytes,
                                   getTxBytes,
                                   networkInterfaceStatsSuccess];

/**
 * handling main thread's get network interface stats request
 */
function getNetworkInterfaceStats(params) {
  debug("getNetworkInterfaceStats: " + params.ifname);

  params.rxBytes = -1;
  params.txBytes = -1;
  params.date = new Date();

  chain(params, gNetworkInterfaceStatsChain, networkInterfaceStatsFail);
  return true;
}

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("Network Worker: " + s + "\n");
  };
} else {
  debug = function (s) {};
}

