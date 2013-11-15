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

const PERSIST_SYS_USB_CONFIG_PROPERTY = "persist.sys.usb.config";
const SYS_USB_CONFIG_PROPERTY         = "sys.usb.config";
const SYS_USB_STATE_PROPERTY          = "sys.usb.state";

const USB_FUNCTION_RNDIS  = "rndis";
const USB_FUNCTION_ADB    = "adb";

const kNetdInterfaceChangedTopic = "netd-interface-change";
const kNetdBandwidthControlTopic = "netd-bandwidth-control";

// Use this command to continue the function chain.
const DUMMY_COMMAND = "tether status";

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

// Broadcast messages
const NETD_COMMAND_INTERFACE_CHANGE     = 600;
const NETD_COMMAND_BANDWIDTH_CONTROLLER = 601;

const INTERFACE_DELIMIT = "\0";

importScripts("systemlibs.js");

const SDK_VERSION = libcutils.property_get("ro.build.version.sdk", "0");

function netdResponseType(code) {
  return Math.floor(code/100)*100;
}

function isBroadcastMessage(code) {
  let type = netdResponseType(code);
  return (type == NETD_COMMAND_UNSOLICITED);
}

function isError(code) {
  let type = netdResponseType(code);
  return (type != NETD_COMMAND_PROCEEDING && type != NETD_COMMAND_OKAY);
}

function isComplete(code) {
  let type = netdResponseType(code);
  return (type != NETD_COMMAND_PROCEEDING);
}

function isProceeding(code) {
  let type = netdResponseType(code);
  return (type === NETD_COMMAND_PROCEEDING);
}

function sendBroadcastMessage(code, reason) {
  let topic = null;
  switch (code) {
    case NETD_COMMAND_INTERFACE_CHANGE:
      topic = "netd-interface-change";
      break;
    case NETD_COMMAND_BANDWIDTH_CONTROLLER:
      topic = "netd-bandwidth-control";
      break;
  }

  if (topic) {
    postMessage({id: 'broadcast', topic: topic, reason: reason});
  }
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
  // This parameter is used to disable ipforwarding.
  params.enable = false;
  chain(params, gUSBFailChain, null);

  // Disable usb rndis function.
  enableUsbRndis({enable: false, report: false});
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

function updateUpStreamSuccess(params) {
  // Notify the main thread.
  postMessage(params);
  return true;
}

function updateUpStreamFail(params) {
  // Notify the main thread.
  postMessage(params);
  return true;
}

function wifiOperationModeFail(params) {
  // Notify the main thread.
  postMessage(params);
  return true;
}

function wifiOperationModeSuccess(params) {
  // Notify the main thread.
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
  return {
    ifname:      ifname,
    gateway_str: libcutils.property_get("net." + ifname + ".gw"),
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
 * Start/Stop DHCP server.
 */
function setDhcpServer(config) {
  function onSuccess() {
    postMessage({ id: config.id, success: true });
    return true;
  }

  function onError() {
    postMessage({ id: config.id, success: false });
  }

  let startDhcpServerChain = [setInterfaceUp,
                              startTethering,
                              onSuccess];

  let stopDhcpServerChain = [stopTethering,
                             onSuccess];

  if (config.enabled) {
    let params = { wifiStartIp: config.startIp,
                   wifiEndIp: config.endIp,
                   ip: config.serverIp,
                   prefix: config.maskLength,
                   ifname: config.ifname,
                   link: "up" };

    chain(params, startDhcpServerChain, onError);
  } else {
    chain({}, stopDhcpServerChain, onError);
  }
}

/**
 * Set DNS servers for given network interface.
 */
function setDNS(options) {
  let ifprops = getIFProperties(options.ifname);
  let dns1_str = options.dns1_str || ifprops.dns1_str;
  let dns2_str = options.dns2_str || ifprops.dns2_str;
  libcutils.property_set("net.dns1", dns1_str);
  libcutils.property_set("net.dns2", dns2_str);
  // Bump the DNS change property.
  let dnschange = libcutils.property_get("net.dnschange", "0");
  libcutils.property_set("net.dnschange", (parseInt(dnschange, 10) + 1).toString());
}

/**
 * Set default route and DNS servers for given network interface.
 */
function setDefaultRouteAndDNS(options) {
  if (options.oldIfname) {
    libnetutils.ifc_remove_default_route(options.oldIfname);
  }

  let ifprops = getIFProperties(options.ifname);
  let gateway_str = options.gateway_str || ifprops.gateway_str;
  let dns1_str = options.dns1_str || ifprops.dns1_str;
  let dns2_str = options.dns2_str || ifprops.dns2_str;
  let gateway = netHelpers.stringToIP(gateway_str);

  libnetutils.ifc_set_default_route(options.ifname, gateway);
  libcutils.property_set("net.dns1", dns1_str);
  libcutils.property_set("net.dns2", dns2_str);

  // Bump the DNS change property.
  let dnschange = libcutils.property_get("net.dnschange", "0");
  libcutils.property_set("net.dnschange", (parseInt(dnschange, 10) + 1).toString());
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
  for (let i = 0; i < options.hostnames.length; i++) {
    libnetutils.ifc_add_route(options.ifname, options.hostnames[i], 32, options.gateway);
  }
}

/**
 * Remove host route for given network interface.
 */
function removeHostRoute(options) {
  for (let i = 0; i < options.hostnames.length; i++) {
    libnetutils.ifc_remove_route(options.ifname, options.hostnames[i], 32, options.gateway);
  }
}

/**
 * Remove the routes associated with the named interface.
 */
function removeHostRoutes(options) {
  libnetutils.ifc_remove_host_routes(options.ifname);
}

function removeNetworkRoute(options) {
  let ipvalue = netHelpers.stringToIP(options.ip);
  let netmaskvalue = netHelpers.stringToIP(options.netmask);
  let subnet = netmaskvalue & ipvalue;
  let dst = netHelpers.ipToString(subnet);
  let prefixLength = netHelpers.getMaskLength(netmaskvalue);
  let gateway = "0.0.0.0";

  libnetutils.ifc_remove_default_route(options.ifname);
  libnetutils.ifc_remove_route(options.ifname, dst, prefixLength, gateway);
}

let gCommandQueue = [];
let gCurrentCommand = null;
let gCurrentCallback = null;
let gPending = false;
let gReason = [];

/**
 * This helper function acts like String.split() fucntion.
 * The function finds the first token in the javascript
 * uint8 type array object, where tokens are delimited by
 * the delimiter. The first token and the index pointer to
 * the next token are returned in this function.
 */
function split(start, data, delimiter) {
  // Sanity check.
  if (start < 0 || data.length <= 0) {
    return null;
  }

  let result = "";
  let i = start;
  while (i < data.length) {
    let octet = data[i];
    i += 1;
    if (octet === delimiter) {
      return {token: result, index: i};
    }
    result += String.fromCharCode(octet);
  }
  return null;
}

/**
 * Handle received data from netd.
 */
function onNetdMessage(data) {
  let result = split(0, data, 32);
  if (!result) {
    nextNetdCommand();
    return;
  }
  let code = parseInt(result.token);

  // Netd response contains the command sequence number
  // in non-broadcast message for Android jb version.
  // The format is ["code" "optional sequence number" "reason"]
  if (!isBroadcastMessage(code) && SDK_VERSION >= 16) {
    result = split(result.index, data, 32);
  }

  let i = result.index;
  let reason = "";
  for (; i < data.length; i++) {
    let octet = data[i];
    reason += String.fromCharCode(octet);
  }

  if (isBroadcastMessage(code)) {
    debug("Receiving broadcast message from netd.");
    debug("          ==> Code: " + code + "  Reason: " + reason);
    sendBroadcastMessage(code, reason);
    nextNetdCommand();
    return;
  }

  // Set pending to false before we handle next command.
  debug("Receiving '" + gCurrentCommand + "' command response from netd.");
  debug("          ==> Code: " + code + "  Reason: " + reason);

  gReason.push(reason);

  // 1xx response code regards as command is proceeding, we need to wait for
  // final response code such as 2xx, 4xx and 5xx before sending next command.
  if (isProceeding(code)) {
    return;
  }

  if (isComplete(code)) {
    gPending = false;
  }

  if (gCurrentCallback) {
    gCurrentCallback(isError(code),
                     {code: code, reason: gReason.join(INTERFACE_DELIMIT)});
    gReason = [];
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

  // Android JB version adds sequence number to netd command.
  let command = (SDK_VERSION >= 16) ? "0 " + gCurrentCommand : gCurrentCommand;
  return postNetdCommand(command);
}

function setInterfaceUp(params, callback) {
  let command = "interface setcfg " + params.ifname + " " + params.ip + " " +
                params.prefix + " ";
  if (SDK_VERSION >= 16) {
    command += params.link;
  } else {
    command += "[" + params.link + "]";
  }
  return doCommand(command, callback);
}

function setInterfaceDown(params, callback) {
  let command = "interface setcfg " + params.ifname + " " + params.ip + " " +
                params.prefix + " ";
  if (SDK_VERSION >= 16) {
    command += params.link;
  } else {
    command += "[" + params.link + "]";
  }
  return doCommand(command, callback);
}

function setIpForwardingEnabled(params, callback) {
  let command;

  if (params.enable) {
    command = "ipfwd enable";
  } else {
    // Don't disable ip forwarding because others interface still need it.
    // Send the dummy command to continue the function chain.
    if ("interfaceList" in params && params.interfaceList.length > 1) {
      command = DUMMY_COMMAND;
    } else {
      command = "ipfwd disable";
    }
  }
  return doCommand(command, callback);
}

function startTethering(params, callback) {
  let command;
  // We don't need to start tethering again.
  // Send the dummy command to continue the function chain.
  if (params.resultReason.indexOf("started") !== -1) {
    command = DUMMY_COMMAND;
  } else {
    command = "tether start " + params.wifiStartIp + " " + params.wifiEndIp;

    // If usbStartIp/usbEndIp is not valid, don't append them since
    // the trailing white spaces will be parsed to extra empty args
    // See: http://androidxref.com/4.3_r2.1/xref/system/core/libsysutils/src/FrameworkListener.cpp#78
    if (params.usbStartIp && params.usbEndIp) {
      command += " " + params.usbStartIp + " " + params.usbEndIp;
    }
  }
  return doCommand(command, callback);
}

function tetheringStatus(params, callback) {
  let command = "tether status";
  return doCommand(command, callback);
}

function stopTethering(params, callback) {
  let command;

  // Don't stop tethering because others interface still need it.
  // Send the dummy to continue the function chain.
  if ("interfaceList" in params && params.interfaceList.length > 1) {
    command = DUMMY_COMMAND;
  } else {
    command = "tether stop";
  }
  return doCommand(command, callback);
}

function tetherInterface(params, callback) {
  let command = "tether interface add " + params.ifname;
  return doCommand(command, callback);
}

function preTetherInterfaceList(params, callback) {
  let command = (SDK_VERSION >= 16) ? "tether interface list"
                                    : "tether interface list 0";
  return doCommand(command, callback);
}

function postTetherInterfaceList(params, callback) {
  params.interfaceList = params.resultReason.split(INTERFACE_DELIMIT);

  // Send the dummy command to continue the function chain.
  let command = DUMMY_COMMAND;
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
  // Skip the command for sdk version >= 16.
  if (SDK_VERSION >= 16) {
    callback(false, {code: "", reason: ""});
    return true;
  }
  let command = "softap start " + params.ifname;
  return doCommand(command, callback);
}

function stopAccessPointDriver(params, callback) {
  // Skip the command for sdk version >= 16.
  if (SDK_VERSION >= 16) {
    callback(false, {code: "", reason: ""});
    return true;
  }
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

function escapeQuote(str) {
  str = str.replace(/\\/g, "\\\\");
  return str.replace(/"/g, "\\\"");
}

/**
 * Command format for sdk version < 16
 *   Arguments:
 *     argv[2] - wlan interface
 *     argv[3] - SSID
 *     argv[4] - Security
 *     argv[5] - Key
 *     argv[6] - Channel
 *     argv[7] - Preamble
 *     argv[8] - Max SCB
 *
 * Command format for sdk version >= 16
 *   Arguments:
 *     argv[2] - wlan interface
 *     argv[3] - SSID
 *     argv[4] - Security
 *     argv[5] - Key
 */
function setAccessPoint(params, callback) {
  let command;
  if (SDK_VERSION >= 16) {
    command = "softap set " + params.ifname +
              " \"" + escapeQuote(params.ssid) + "\"" +
              " " + params.security +
              " \"" + escapeQuote(params.key) + "\"";
  } else {
    command = "softap set " + params.ifname +
              " " + params.wifictrlinterfacename +
              " \"" + escapeQuote(params.ssid) + "\"" +
              " " + params.security +
              " \"" + escapeQuote(params.key) + "\"" +
              " " + "6 0 8";
  }
  return doCommand(command, callback);
}

function cleanUpStream(params, callback) {
  let command = "nat disable " + params.previous.internalIfname + " " +
                params.previous.externalIfname + " " + "0";
  return doCommand(command, callback);
}

function createUpStream(params, callback) {
  let command = "nat enable " + params.current.internalIfname + " " +
                params.current.externalIfname + " " + "0";
  return doCommand(command, callback);
}

/**
 * Modify usb function's property to turn on USB RNDIS function
 */
function enableUsbRndis(params) {
  let report = params.report;
  let retry = 0;

  // For some reason, rndis doesn't play well with diag,modem,nmea.
  // So when turning rndis on, we set sys.usb.config to either "rndis"
  // or "rndis,adb". When turning rndis off, we go back to
  // persist.sys.usb.config.
  //
  // On the otoro/unagi, persist.sys.usb.config should be one of:
  //
  //    diag,modem,nmea,mass_storage
  //    diag,modem,nmea,mass_storage,adb
  //
  // When rndis is enabled, sys.usb.config should be one of:
  //
  //    rdnis
  //    rndis,adb
  //
  // and when rndis is disabled, it should revert to persist.sys.usb.config

  let currentConfig = libcutils.property_get(SYS_USB_CONFIG_PROPERTY);
  let configFuncs = currentConfig.split(",");
  let persistConfig = libcutils.property_get(PERSIST_SYS_USB_CONFIG_PROPERTY);
  let persistFuncs = persistConfig.split(",");

  if (params.enable) {
    configFuncs = [USB_FUNCTION_RNDIS];
    if (persistFuncs.indexOf(USB_FUNCTION_ADB) >= 0) {
      configFuncs.push(USB_FUNCTION_ADB);
    }
  } else {
    // We're turning rndis off, revert back to the persist setting.
    // adb will already be correct there, so we don't need to do any
    // further adjustments.
    configFuncs = persistFuncs;
  }
  let newConfig = configFuncs.join(",");
  if (newConfig != currentConfig) {
    libcutils.property_set(SYS_USB_CONFIG_PROPERTY, newConfig);
  }

  // Trigger the timer to check usb state and report the result to NetworkManager.
  if (report) {
    setTimeout(checkUsbRndisState, USB_FUNCTION_RETRY_INTERVAL, params);
  }

  function checkUsbRndisState(params) {
    let currentState = libcutils.property_get(SYS_USB_STATE_PROPERTY);
    let stateFuncs = currentState.split(",");
    let rndisPresent = (stateFuncs.indexOf(USB_FUNCTION_RNDIS) >= 0);
    if (params.enable == rndisPresent) {
      params.result = true;
      postMessage(params);
      retry = 0;
      return;
    }
    if (retry < USB_FUNCTION_RETRY_TIMES) {
      retry++;
      setTimeout(checkUsbRndisState, USB_FUNCTION_RETRY_INTERVAL, params);
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
  debug("     wifiStartIp: " + params.wifiStartIp);
  debug("     wifiEndIp: " + params.wifiEndIp);
  debug("     usbStartIp: " + params.usbStartIp);
  debug("     usbEndIp: " + params.usbEndIp);
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
                        tetheringStatus,
                        startTethering,
                        setDnsForwarders,
                        enableNat,
                        wifiTetheringSuccess];

let gWifiDisableChain = [stopSoftAP,
                         stopAccessPointDriver,
                         wifiFirmwareReload,
                         untetherInterface,
                         preTetherInterfaceList,
                         postTetherInterfaceList,
                         disableNat,
                         setIpForwardingEnabled,
                         stopTethering,
                         wifiTetheringSuccess];

/**
 * handling main thread's enable/disable WiFi Tethering request
 */
function setWifiTethering(params) {
  let enable = params.enable;
  let interfaceProperties = getIFProperties(params.externalIfname);

  if (interfaceProperties.dns1_str) {
    params.dns1 = interfaceProperties.dns1_str;
  }
  if (interfaceProperties.dns2_str) {
    params.dns2 = interfaceProperties.dns2_str;
  }
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

let gUpdateUpStreamChain = [cleanUpStream,
                            createUpStream,
                            updateUpStreamSuccess];
/**
 * handling upstream interface change event.
 */
function updateUpStream(params) {
  chain(params, gUpdateUpStreamChain, updateUpStreamFail);
}

let gUSBEnableChain = [setInterfaceUp,
                       enableNat,
                       setIpForwardingEnabled,
                       tetherInterface,
                       tetheringStatus,
                       startTethering,
                       setDnsForwarders,
                       usbTetheringSuccess];

let gUSBDisableChain = [untetherInterface,
                        preTetherInterfaceList,
                        postTetherInterfaceList,
                        disableNat,
                        setIpForwardingEnabled,
                        stopTethering,
                        usbTetheringSuccess];

/**
 * handling main thread's enable/disable USB Tethering request
 */
function setUSBTethering(params) {
  let enable = params.enable;
  let interfaceProperties = getIFProperties(params.externalIfname);

  if (interfaceProperties.dns1_str) {
    params.dns1 = interfaceProperties.dns1_str;
  }
  if (interfaceProperties.dns2_str) {
    params.dns2 = interfaceProperties.dns2_str;
  }

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

let gWifiOperationModeChain = [wifiFirmwareReload,
                               wifiOperationModeSuccess];

/**
 * handling main thread's reload Wifi firmware request
 */
function setWifiOperationMode(params) {
  debug("setWifiOperationMode: " + params.ifname + " " + params.mode);
  chain(params, gWifiOperationModeChain, wifiOperationModeFail);
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

