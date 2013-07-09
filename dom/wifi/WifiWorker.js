/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var DEBUG = false; // set to true to show debug messages.

const WIFIWORKER_CONTRACTID = "@mozilla.org/wifi/worker;1";
const WIFIWORKER_CID        = Components.ID("{a14e8977-d259-433a-a88d-58dd44657e5b}");

const WIFIWORKER_WORKER     = "resource://gre/modules/wifi_worker.js";

const kNetworkInterfaceStateChangedTopic = "network-interface-state-changed";
const kMozSettingsChangedObserverTopic   = "mozsettings-changed";

const MAX_RETRIES_ON_AUTHENTICATION_FAILURE = 2;
const MAX_SUPPLICANT_LOOP_ITERATIONS = 4;

// Settings DB path for wifi
const SETTINGS_WIFI_ENABLED            = "wifi.enabled";
const SETTINGS_WIFI_DEBUG_ENABLED      = "wifi.debugging.enabled";
// Settings DB path for Wifi tethering.
const SETTINGS_WIFI_TETHERING_ENABLED  = "tethering.wifi.enabled";
const SETTINGS_WIFI_SSID               = "tethering.wifi.ssid";
const SETTINGS_WIFI_SECURITY_TYPE      = "tethering.wifi.security.type";
const SETTINGS_WIFI_SECURITY_PASSWORD  = "tethering.wifi.security.password";
const SETTINGS_WIFI_IP                 = "tethering.wifi.ip";
const SETTINGS_WIFI_PREFIX             = "tethering.wifi.prefix";
const SETTINGS_WIFI_DHCPSERVER_STARTIP = "tethering.wifi.dhcpserver.startip";
const SETTINGS_WIFI_DHCPSERVER_ENDIP   = "tethering.wifi.dhcpserver.endip";
const SETTINGS_WIFI_DNS1               = "tethering.wifi.dns1";
const SETTINGS_WIFI_DNS2               = "tethering.wifi.dns2";

// Settings DB path for USB tethering.
const SETTINGS_USB_DHCPSERVER_STARTIP  = "tethering.usb.dhcpserver.startip";
const SETTINGS_USB_DHCPSERVER_ENDIP    = "tethering.usb.dhcpserver.endip";

// Default value for WIFI tethering.
const DEFAULT_WIFI_IP                  = "192.168.1.1";
const DEFAULT_WIFI_PREFIX              = "24";
const DEFAULT_WIFI_DHCPSERVER_STARTIP  = "192.168.1.10";
const DEFAULT_WIFI_DHCPSERVER_ENDIP    = "192.168.1.30";
const DEFAULT_WIFI_SSID                = "FirefoxHotspot";
const DEFAULT_WIFI_SECURITY_TYPE       = "open";
const DEFAULT_WIFI_SECURITY_PASSWORD   = "1234567890";
const DEFAULT_DNS1                     = "8.8.8.8";
const DEFAULT_DNS2                     = "8.8.4.4";

// Default value for USB tethering.
const DEFAULT_USB_DHCPSERVER_STARTIP   = "192.168.0.10";
const DEFAULT_USB_DHCPSERVER_ENDIP     = "192.168.0.30";

const WIFI_FIRMWARE_AP            = "AP";
const WIFI_FIRMWARE_STATION       = "STA";
const WIFI_SECURITY_TYPE_NONE     = "open";
const WIFI_SECURITY_TYPE_WPA_PSK  = "wpa-psk";
const WIFI_SECURITY_TYPE_WPA2_PSK = "wpa2-psk";

const NETWORK_INTERFACE_UP   = "up";
const NETWORK_INTERFACE_DOWN = "down";

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkManager",
                                   "@mozilla.org/network/manager;1",
                                   "nsINetworkManager");

XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

// A note about errors and error handling in this file:
// The libraries that we use in this file are intended for C code. For
// C code, it is natural to return -1 for errors and 0 for success.
// Therefore, the code that interacts directly with the worker uses this
// convention (note: command functions do get boolean results since the
// command always succeeds and we do a string/boolean check for the
// expected results).
var WifiManager = (function() {
  function getStartupPrefs() {
    Cu.import("resource://gre/modules/systemlibs.js");
    return {
      sdkVersion: parseInt(libcutils.property_get("ro.build.version.sdk"), 10),
      unloadDriverEnabled: libcutils.property_get("ro.moz.wifi.unloaddriver") === "1",
      schedScanRecovery: libcutils.property_get("ro.moz.wifi.sched_scan_recover") === "false" ? false : true
    };
  }

  let {sdkVersion, unloadDriverEnabled, schedScanRecovery} = getStartupPrefs();

  var controlWorker = new ChromeWorker(WIFIWORKER_WORKER);
  var eventWorker = new ChromeWorker(WIFIWORKER_WORKER);

  var manager = {};
  manager.schedScanRecovery = schedScanRecovery;

  // Callbacks to invoke when a reply arrives from the controlWorker.
  var controlCallbacks = Object.create(null);
  var idgen = 0;

  function controlMessage(obj, callback) {
    var id = idgen++;
    obj.id = id;
    if (callback)
      controlCallbacks[id] = callback;
    controlWorker.postMessage(obj);
  }

  function onerror(e) {
    // It is very important to call preventDefault on the event here.
    // If an exception is thrown on the worker, it bubbles out to the
    // component that created it. If that component doesn't have an
    // onerror handler, the worker will try to call the error reporter
    // on the context it was created on. However, That doesn't work
    // for component contexts and can result in crashes. This onerror
    // handler has to make sure that it calls preventDefault on the
    // incoming event.
    e.preventDefault();

    var worker = (this === controlWorker) ? "control" : "event";

    debug("Got an error from the " + worker + " worker: " + e.filename +
          ":" + e.lineno + ": " + e.message + "\n");
  }

  controlWorker.onerror = onerror;
  eventWorker.onerror = onerror;

  controlWorker.onmessage = function(e) {
    var data = e.data;
    var id = data.id;
    var callback = controlCallbacks[id];
    if (callback) {
      callback(data);
      delete controlCallbacks[id];
    }
  };

  // Polling the status worker
  var recvErrors = 0;
  eventWorker.onmessage = function(e) {
    // Process the event and tell the event worker to listen for more events.
    if (handleEvent(e.data.event))
      waitForEvent();
  };

  function waitForEvent() {
    eventWorker.postMessage({ cmd: "wait_for_event" });
  }

  // Commands to the control worker.

  function voidControlMessage(cmd, callback) {
    controlMessage({ cmd: cmd }, function (data) {
      callback(data.status);
    });
  }

  var driverLoaded = false;
  function loadDriver(callback) {
    if (driverLoaded) {
      callback(0);
      return;
    }

    voidControlMessage("load_driver", function(status) {
      driverLoaded = (status >= 0);
      callback(status)
    });
  }

  function unloadDriver(callback) {
    if (!unloadDriverEnabled) {
      // Unloading drivers is generally unnecessary and
      // can trigger bugs in some drivers.
      // On properly written drivers, bringing the interface
      // down powers down the interface.
      callback(0);
      return;
    }

    voidControlMessage("unload_driver", function(status) {
      driverLoaded = (status < 0);
      callback(status);
    });
  }

  function startSupplicant(callback) {
    voidControlMessage("start_supplicant", callback);
  }

  function terminateSupplicant(callback) {
    doBooleanCommand("TERMINATE", "OK", callback);
  }

  function stopSupplicant(callback) {
    voidControlMessage("stop_supplicant", callback);
  }

  function connectToSupplicant(callback) {
    voidControlMessage("connect_to_supplicant", callback);
  }

  function closeSupplicantConnection(callback) {
    voidControlMessage("close_supplicant_connection", callback);
  }

  function doCommand(request, callback) {
    controlMessage({ cmd: "command", request: request }, callback);
  }

  function doIntCommand(request, callback) {
    doCommand(request, function(data) {
      callback(data.status ? -1 : (data.reply|0));
    });
  }

  function doBooleanCommand(request, expected, callback) {
    doCommand(request, function(data) {
      callback(data.status ? false : (data.reply == expected));
    });
  }

  function doStringCommand(request, callback) {
    doCommand(request, function(data) {
      callback(data.status ? null : data.reply);
    });
  }

  function listNetworksCommand(callback) {
    doStringCommand("LIST_NETWORKS", callback);
  }

  function addNetworkCommand(callback) {
    doIntCommand("ADD_NETWORK", callback);
  }

  function setNetworkVariableCommand(netId, name, value, callback) {
    doBooleanCommand("SET_NETWORK " + netId + " " + name + " " + value, "OK", callback);
  }

  function getNetworkVariableCommand(netId, name, callback) {
    doStringCommand("GET_NETWORK " + netId + " " + name, callback);
  }

  function removeNetworkCommand(netId, callback) {
    doBooleanCommand("REMOVE_NETWORK " + netId, "OK", callback);
  }

  function enableNetworkCommand(netId, disableOthers, callback) {
    doBooleanCommand((disableOthers ? "SELECT_NETWORK " : "ENABLE_NETWORK ") + netId, "OK", callback);
  }

  function disableNetworkCommand(netId, callback) {
    doBooleanCommand("DISABLE_NETWORK " + netId, "OK", callback);
  }

  function statusCommand(callback) {
    doStringCommand("STATUS", callback);
  }

  function pingCommand(callback) {
    doBooleanCommand("PING", "PONG", callback);
  }

  function scanResultsCommand(callback) {
    doStringCommand("SCAN_RESULTS", callback);
  }

  function disconnectCommand(callback) {
    doBooleanCommand("DISCONNECT", "OK", callback);
  }

  function reconnectCommand(callback) {
    doBooleanCommand("RECONNECT", "OK", callback);
  }

  function reassociateCommand(callback) {
    doBooleanCommand("REASSOCIATE", "OK", callback);
  }

  // A note about background scanning:
  // Normally, background scanning shouldn't be necessary as wpa_supplicant
  // has the capability to automatically schedule its own scans at appropriate
  // intervals. However, with some drivers, this appears to get stuck after
  // three scans, so we enable the driver's background scanning to work around
  // that when we're not connected to any network. This ensures that we'll
  // automatically reconnect to networks if one falls out of range.
  var reEnableBackgroundScan = false;

  // NB: This is part of the internal API.
  manager.backgroundScanEnabled = false;
  function setBackgroundScan(enable, callback) {
    var doEnable = (enable === "ON");
    if (doEnable === manager.backgroundScanEnabled) {
      callback(false, true);
      return;
    }

    manager.backgroundScanEnabled = doEnable;
    doBooleanCommand("SET pno " + (manager.backgroundScanEnabled ? "1" : "0"),
                     "OK",
                     function(ok) {
                       callback(true, ok);
                     });
  }

  var scanModeActive = false;

  function doSetScanModeCommand(setActive, callback) {
    doBooleanCommand(setActive ? "DRIVER SCAN-ACTIVE" : "DRIVER SCAN-PASSIVE", "OK", callback);
  }

  function scanCommand(forceActive, callback) {
    if (forceActive && !scanModeActive) {
      // Note: we ignore errors from doSetScanMode.
      doSetScanModeCommand(true, function(ignore) {
        setBackgroundScan("OFF", function(turned, ignore) {
          reEnableBackgroundScan = turned;
          doBooleanCommand("SCAN", "OK", function(ok) {
            doSetScanModeCommand(false, function(ignore) {
              // The result of scanCommand is the result of the actual SCAN
              // request.
              callback(ok);
            });
          });
        });
      });
      return;
    }
    doBooleanCommand("SCAN", "OK", callback);
  }

  var debugEnabled = false;
  function setLogLevel(level, callback) {
    doBooleanCommand("LOG_LEVEL " + level, "OK", callback);
  }

  function syncDebug() {
    if (debugEnabled !== DEBUG) {
      let wanted = DEBUG;
      setLogLevel(wanted ? "DEBUG" : "INFO", function(ok) {
        if (ok)
          debugEnabled = wanted;
      });
    }
  }

  function getLogLevel(callback) {
    doStringCommand("LOG_LEVEL", callback);
  }

  function getDebugEnabled(callback) {
    getLogLevel(function(level) {
      if (level === null) {
        debug("Unable to get wpa_supplicant's log level");
        callback(false);
        return;
      }

      var lines = level.split("\n");
      for (let i = 0; i < lines.length; ++i) {
        let match = /Current level: (.*)/.exec(lines[i]);
        if (match) {
          debugEnabled = match[1].toLowerCase() === "debug";
          callback(true);
          return;
        }
      }

      // If we're here, we didn't get the current level.
      callback(false);
    });
  }

  function setScanModeCommand(setActive, callback) {
    scanModeActive = setActive;
    doSetScanModeCommand(setActive, callback);
  }

  function wpsPbcCommand(callback) {
    doBooleanCommand("WPS_PBC", "OK", callback);
  }

  function wpsPinCommand(detail, callback) {
    doStringCommand("WPS_PIN " +
                    (detail.bssid === undefined ? "any" : detail.bssid) +
                    (detail.pin === undefined ? "" : (" " + detail.pin)),
                    callback);
  }

  function wpsCancelCommand(callback) {
    doBooleanCommand("WPS_CANCEL", "OK", callback);
  }

  function startDriverCommand(callback) {
    doBooleanCommand("DRIVER START", "OK");
  }

  function stopDriverCommand(callback) {
    doBooleanCommand("DRIVER STOP", "OK");
  }

  function startPacketFiltering(callback) {
    doBooleanCommand("DRIVER RXFILTER-ADD 0", "OK", function(ok) {
      ok && doBooleanCommand("DRIVER RXFILTER-ADD 1", "OK", function(ok) {
        ok && doBooleanCommand("DRIVER RXFILTER-ADD 3", "OK", function(ok) {
          ok && doBooleanCommand("DRIVER RXFILTER-START", "OK", callback)
        });
      });
    });
  }

  function stopPacketFiltering(callback) {
    doBooleanCommand("DRIVER RXFILTER-STOP", "OK", function(ok) {
      ok && doBooleanCommand("DRIVER RXFILTER-REMOVE 3", "OK", function(ok) {
        ok && doBooleanCommand("DRIVER RXFILTER-REMOVE 1", "OK", function(ok) {
          ok && doBooleanCommand("DRIVER RXFILTER-REMOVE 0", "OK", callback)
        });
      });
    });
  }

  function doGetRssiCommand(cmd, callback) {
    doCommand(cmd, function(data) {
      var rssi = -200;

      if (!data.status) {
        // If we are associating, the reply is "OK".
        var reply = data.reply;
        if (reply != "OK") {
          // Format is: <SSID> rssi XX". SSID can contain spaces.
          var offset = reply.lastIndexOf("rssi ");
          if (offset !== -1)
            rssi = reply.substr(offset + 5) | 0;
        }
      }
      callback(rssi);
    });
  }

  function getRssiCommand(callback) {
    doGetRssiCommand("DRIVER RSSI", callback);
  }

  function getRssiApproxCommand(callback) {
    doGetRssiCommand("DRIVER RSSI-APPROX", callback);
  }

  function getLinkSpeedCommand(callback) {
    doStringCommand("DRIVER LINKSPEED", function(reply) {
      if (reply)
        reply = reply.split(" ")[1] | 0; // Format: LinkSpeed XX
      callback(reply);
    });
  }

  function getConnectionInfoGB(callback) {
    var rval = {};
    getRssiApproxCommand(function(rssi) {
      rval.rssi = rssi;
      getLinkSpeedCommand(function(linkspeed) {
        rval.linkspeed = linkspeed;
        callback(rval);
      });
    });
  }

  function getConnectionInfoICS(callback) {
    doStringCommand("SIGNAL_POLL", function(reply) {
      if (!reply) {
        callback(null);
        return;
      }

      let rval = {};
      var lines = reply.split("\n");
      for (let i = 0; i < lines.length; ++i) {
        let [key, value] = lines[i].split("=");
        switch (key.toUpperCase()) {
          case "RSSI":
            rval.rssi = value | 0;
            break;
          case "LINKSPEED":
            rval.linkspeed = value | 0;
            break;
          default:
            // Ignore.
        }
      }

      callback(rval);
    });
  }

  function getMacAddressCommand(callback) {
    doStringCommand("DRIVER MACADDR", function(reply) {
      if (reply)
        reply = reply.split(" ")[2]; // Format: Macaddr = XX.XX.XX.XX.XX.XX
      callback(reply);
    });
  }

  function setPowerModeCommandICS(mode, callback) {
    doBooleanCommand("DRIVER POWERMODE " + (mode === "AUTO" ? 0 : 1), "OK", callback);
  }

  function setPowerModeCommandJB(mode, callback) {
    doBooleanCommand("SET ps " + (mode === "AUTO" ? 1 : 0), "OK", callback);
  }

  function getPowerModeCommand(callback) {
    doStringCommand("DRIVER GETPOWER", function(reply) {
      if (reply)
        reply = (reply.split()[2]|0); // Format: powermode = XX
      callback(reply);
    });
  }

  function setNumAllowedChannelsCommand(numChannels, callback) {
    doBooleanCommand("DRIVER SCAN-CHANNELS " + numChannels, "OK", callback);
  }

  function getNumAllowedChannelsCommand(callback) {
    doStringCommand("DRIVER SCAN-CHANNELS", function(reply) {
      if (reply)
        reply = (reply.split()[2]|0); // Format: Scan-Channels = X
      callback(reply);
    });
  }

  function setBluetoothCoexistenceModeCommand(mode, callback) {
    doBooleanCommand("DRIVER BTCOEXMODE " + mode, "OK", callback);
  }

  function setBluetoothCoexistenceScanModeCommand(mode, callback) {
    doBooleanCommand("DRIVER BTCOEXSCAN-" + (mode ? "START" : "STOP"), "OK", callback);
  }

  function saveConfigCommand(callback) {
    // Make sure we never write out a value for AP_SCAN other than 1
    doBooleanCommand("AP_SCAN 1", "OK", function(ok) {
      doBooleanCommand("SAVE_CONFIG", "OK", callback);
    });
  }

  function reloadConfigCommand(callback) {
    doBooleanCommand("RECONFIGURE", "OK", callback);
  }

  function setScanResultHandlingCommand(mode, callback) {
    doBooleanCommand("AP_SCAN " + mode, "OK", callback);
  }

  function addToBlacklistCommand(bssid, callback) {
    doBooleanCommand("BLACKLIST " + bssid, "OK", callback);
  }

  function clearBlacklistCommand(callback) {
    doBooleanCommand("BLACKLIST clear", "OK", callback);
  }

  function setSuspendOptimizationsCommand(enabled, callback) {
    doBooleanCommand("DRIVER SETSUSPENDOPT " + (enabled ? 0 : 1), "OK", callback);
  }

  function getProperty(key, defaultValue, callback) {
    controlMessage({ cmd: "property_get", key: key, defaultValue: defaultValue }, function(data) {
      callback(data.status < 0 ? null : data.value);
    });
  }

  function setProperty(key, value, callback) {
    controlMessage({ cmd: "property_set", key: key, value: value }, function(data) {
      callback(!data.status);
    });
  }

  function enableInterface(ifname, callback) {
    controlMessage({ cmd: "ifc_enable", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  }

  function disableInterface(ifname, callback) {
    controlMessage({ cmd: "ifc_disable", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  }

  function addHostRoute(ifname, route, callback) {
    controlMessage({ cmd: "ifc_add_host_route", ifname: ifname, route: route }, function(data) {
      callback(!data.status);
    });
  }

  function removeHostRoutes(ifname, callback) {
    controlMessage({ cmd: "ifc_remove_host_routes", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  }

  function setDefaultRoute(ifname, route, callback) {
    controlMessage({ cmd: "ifc_set_default_route", ifname: ifname, route: route }, function(data) {
      callback(!data.status);
    });
  }

  function getDefaultRoute(ifname, callback) {
    controlMessage({ cmd: "ifc_get_default_route", ifname: ifname }, function(data) {
      callback(!data.route);
    });
  }

  function removeDefaultRoute(ifname, callback) {
    controlMessage({ cmd: "ifc_remove_default_route", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  }

  function resetConnections(ifname, callback) {
    controlMessage({ cmd: "ifc_reset_connections", ifname: ifname }, function(data) {
      callback(!data.status);
    });
  }

  var staticIpConfig = Object.create(null);
  function setStaticIpMode(network, info, callback) {
    let setNetworkKey = getNetworkKey(network);
    let curNetworkKey = null;
    let currentNetwork = Object.create(null);
    currentNetwork.netId = manager.connectionInfo.id;

    manager.getNetworkConfiguration(currentNetwork, function (){
      curNetworkKey = getNetworkKey(currentNetwork);

      // Add additional information to static ip configuration
      // It is used to compatiable with information dhcp callback.
      info.ipaddr = stringToIp(info.ipaddr_str);
      info.gateway = stringToIp(info.gateway_str);
      info.mask_str = makeMask(info.maskLength);

      // Optional
      info.dns1 = stringToIp("dns1_str" in info ? info.dns1_str : "");
      info.dns2 = stringToIp("dns2_str" in info ? info.dns2_str : "");
      info.proxy = stringToIp("proxy_str" in info ? info.proxy_str : "");

      staticIpConfig[setNetworkKey] = info;

      // If the ssid of current connection is the same as configured ssid
      // It means we need update current connection to use static IP address.
      if (setNetworkKey == curNetworkKey) {
        // Use configureInterface directly doesn't work, the network iterface
        // and routing table is changed but still cannot connect to network
        // so the workaround here is disable interface the enable again to
        // trigger network reconnect with static ip.
        disableInterface(manager.ifname, function (ok) {
          enableInterface(manager.ifname, function (ok) {
          });
        });
      }
    });
  }

  var dhcpInfo = null;
  function runDhcp(ifname) {
    debug("Run Dhcp");
    controlMessage({ cmd: "dhcp_do_request", ifname: ifname }, function(data) {
      dhcpInfo = data.status ? null : data;
      runIpConfig(ifname, dhcpInfo);
    });
  }

  function runStaticIp(ifname, key) {
    debug("Run static ip");

    // Read static ip information from settings.
    let staticIpInfo;

    if (!(key in staticIpConfig))
      return;

    staticIpInfo = staticIpConfig[key];

    // Stop dhcpd when use static IP
    if (dhcpInfo != null) {
      stopDhcp(manager.ifname, function() {});
    }

    // Set ip, mask length, gateway, dns to network interface
    configureInterface(ifname,
                       staticIpInfo.ipaddr,
                       staticIpInfo.maskLength,
                       staticIpInfo.gateway,
                       staticIpInfo.dns1,
                       staticIpInfo.dns2, function (data) {
      runIpConfig(ifname, staticIpInfo);
    });
  }

  function stopDhcp(ifname, callback) {
    controlMessage({ cmd: "dhcp_stop", ifname: ifname }, function(data) {
      dhcpInfo = null;
      notify("dhcplost");
      callback(!data.status);
    });
  }

  function releaseDhcpLease(ifname, callback) {
    controlMessage({ cmd: "dhcp_release_lease", ifname: ifname }, function(data) {
      dhcpInfo = null;
      notify("dhcplost");
      callback(!data.status);
    });
  }

  function getDhcpError(callback) {
    controlMessage({ cmd: "dhcp_get_errmsg" }, function(data) {
      callback(data.error);
    });
  }

  function configureInterface(ifname, ipaddr, mask, gateway, dns1, dns2, callback) {
    controlMessage({ cmd: "ifc_configure", ifname: ifname,
                     ipaddr: ipaddr, mask: mask, gateway: gateway,
                     dns1: dns1, dns2: dns2}, function(data) {
      callback(!data.status);
    });
  }

  function runDhcpRenew(ifname, callback) {
    controlMessage({ cmd: "dhcp_do_request", ifname: ifname }, function(data) {
      if (!data.status)
        dhcpInfo = data;
      callback(data.status ? null : data);
    });
  }

  var suppressEvents = false;
  function notify(eventName, eventObject) {
    if (suppressEvents)
      return;
    var handler = manager["on" + eventName];
    if (handler) {
      if (!eventObject)
        eventObject = ({});
      handler.call(eventObject);
    }
  }

  function notifyStateChange(fields) {
    // If we're already in the COMPLETED state, we might receive events from
    // the supplicant that tell us that we're re-authenticating or reminding
    // us that we're associated to a network. In those cases, we don't need to
    // do anything, so just ignore them.
    if (manager.state === "COMPLETED" &&
        fields.state !== "DISCONNECTED" &&
        fields.state !== "INTERFACE_DISABLED" &&
        fields.state !== "INACTIVE" &&
        fields.state !== "SCANNING") {
      return false;
    }

    // Stop background scanning if we're trying to connect to a network.
    if (manager.backgroundScanEnabled &&
        (fields.state === "ASSOCIATING" ||
         fields.state === "ASSOCIATED" ||
         fields.state === "FOUR_WAY_HANDSHAKE" ||
         fields.state === "GROUP_HANDSHAKE" ||
         fields.state === "COMPLETED")) {
      setBackgroundScan("OFF", function() {});
    }
    fields.prevState = manager.state;
    manager.state = fields.state;

    // Detect wpa_supplicant's loop iterations.
    manager.supplicantLoopDetection(fields.prevState, fields.state);
    notify("statechange", fields);
    return true;
  }

  function parseStatus(status) {
    if (status === null) {
      debug("Unable to get wpa supplicant's status");
      return;
    }

    var ssid;
    var bssid;
    var state;
    var ip_address;
    var id;

    var lines = status.split("\n");
    for (let i = 0; i < lines.length; ++i) {
      let [key, value] = lines[i].split("=");
      switch (key) {
        case "wpa_state":
          state = value;
          break;
        case "ssid":
          ssid = value;
          break;
        case "bssid":
          bssid = value;
          break;
        case "ip_address":
          ip_address = value;
          break;
        case "id":
          id = value;
          break;
      }
    }

    if (bssid && ssid) {
      manager.connectionInfo.bssid = bssid;
      manager.connectionInfo.ssid = ssid;
      manager.connectionInfo.id = id;
    }

    if (ip_address)
      dhcpInfo = { ip_address: ip_address };

    notifyStateChange({ state: state, fromStatus: true });

    // If we parse the status and the supplicant has already entered the
    // COMPLETED state, then we need to set up DHCP right away.
    if (state === "COMPLETED")
      onconnected();
  }

  // try to connect to the supplicant
  var connectTries = 0;
  var retryTimer = null;
  function connectCallback(ok) {
    if (ok === 0) {
      // Tell the event worker to start waiting for events.
      retryTimer = null;
      didConnectSupplicant(function(){});
      return;
    }
    if (connectTries++ < 3) {
      // Try again in 5 seconds.
      if (!retryTimer)
        retryTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

      retryTimer.initWithCallback(function(timer) {
        connectToSupplicant(connectCallback);
      }, 5000, Ci.nsITimer.TYPE_ONE_SHOT);
      return;
    }

    retryTimer = null;
    notify("supplicantlost", { success: false });
  }

  manager.connectionDropped = function(callback) {
    // Reset network interface when connection drop
    configureInterface(manager.ifname, 0, 0, 0, 0, 0, function (data) {
    });

    // If we got disconnected, kill the DHCP client in preparation for
    // reconnection.
    resetConnections(manager.ifname, function() {
      stopDhcp(manager.ifname, function() {
        callback();
      });
    });
  }

  manager.start = function() {
    debug("detected SDK version " + sdkVersion);
    connectToSupplicant(connectCallback);
  }

  function onconnected() {
    // For now we do our own DHCP. In the future, this should be handed
    // off to the Network Manager.
    let currentNetwork = Object.create(null);
    currentNetwork.netId = manager.connectionInfo.id;

    manager.getNetworkConfiguration(currentNetwork, function (){
      let key = getNetworkKey(currentNetwork);
      if (staticIpConfig  &&
          (key in staticIpConfig) &&
          staticIpConfig[key].enabled) {
          debug("Run static ip");
          runStaticIp(manager.ifname, key);
          return;
      }
      runDhcp(manager.ifname);
    });
  }

  function runIpConfig(name, data) {
    if (!data) {
      debug("IP config failed to run");
      notify("networkconnected", { info: data });
      return;
    }

    setProperty("net." + name + ".dns1", ipToString(data.dns1),
                function(ok) {
      if (!ok) {
        debug("Unable to set net.<ifname>.dns1");
        return;
      }
      setProperty("net." + name + ".dns2", ipToString(data.dns2),
                  function(ok) {
        if (!ok) {
          debug("Unable to set net.<ifname>.dns2");
          return;
        }
        setProperty("net." + name + ".gw", ipToString(data.gateway),
                    function(ok) {
          if (!ok) {
            debug("Unable to set net.<ifname>.gw");
            return;
          }
          notify("networkconnected", { info: data });
        });
      });
    });
  }

  var supplicantStatesMap = (sdkVersion >= 15) ?
    ["DISCONNECTED", "INTERFACE_DISABLED", "INACTIVE", "SCANNING",
     "AUTHENTICATING", "ASSOCIATING", "ASSOCIATED", "FOUR_WAY_HANDSHAKE",
     "GROUP_HANDSHAKE", "COMPLETED"]
    :
    ["DISCONNECTED", "INACTIVE", "SCANNING", "ASSOCIATING",
     "ASSOCIATED", "FOUR_WAY_HANDSHAKE", "GROUP_HANDSHAKE",
     "COMPLETED", "DORMANT", "UNINITIALIZED"];

  var driverEventMap = { STOPPED: "driverstopped", STARTED: "driverstarted", HANGED: "driverhung" };

  manager.getCurrentNetworkId = function (ssid, callback) {
    manager.getConfiguredNetworks(function(networks) {
      if (!networks) {
        debug("Unable to get configured networks");
        return callback(null);
      }
      for (let net in networks) {
        let network = networks[net];
        // Trying to get netId from
        // 1. CURRENT network.
        // 2. Trying to associate with SSID 'ssid' event
        if (network.status === "CURRENT" ||
            (ssid && ssid === dequote(network.ssid))) {
          return callback(net);
        }
      }
      callback(null);
    });
  }

  // Handle events sent to us by the event worker.
  function handleEvent(event) {
    debug("Event coming in: " + event);
    if (event.indexOf("CTRL-EVENT-") !== 0 && event.indexOf("WPS") !== 0) {
      // Handle connection fail exception on WEP-128, while password length
      // is not 5 nor 13 bytes.
      if (event.indexOf("Association request to the driver failed") !== -1) {
        notify("passwordmaybeincorrect");
        if (manager.authenticationFailuresCount > MAX_RETRIES_ON_AUTHENTICATION_FAILURE) {
          manager.authenticationFailuresCount = 0;
          notify("disconnected", {ssid: manager.connectionInfo.ssid});
        }
        return true;
      }

      if (event.indexOf("WPA:") == 0 &&
          event.indexOf("pre-shared key may be incorrect") != -1) {
        notify("passwordmaybeincorrect");
      }

      // This is ugly, but we need to grab the SSID here. BSSID is not guaranteed
      // to be provided, so don't grab BSSID here.
      var match = /Trying to associate with.*SSID[ =]'(.*)'/.exec(event);
      if (match) {
        debug("Matched: " + match[1] + "\n");
        manager.connectionInfo.ssid = match[1];
      }
      return true;
    }

    var space = event.indexOf(" ");
    var eventData = event.substr(0, space + 1);
    if (eventData.indexOf("CTRL-EVENT-STATE-CHANGE") === 0) {
      // Parse the event data.
      var fields = {};
      var tokens = event.substr(space + 1).split(" ");
      for (var n = 0; n < tokens.length; ++n) {
        var kv = tokens[n].split("=");
        if (kv.length === 2)
          fields[kv[0]] = kv[1];
      }
      if (!("state" in fields))
        return true;
      fields.state = supplicantStatesMap[fields.state];

      // The BSSID field is only valid in the ASSOCIATING and ASSOCIATED
      // states, except when we "reauth", except this seems to depend on the
      // driver, so simply check to make sure that we don't have a null BSSID.
      if (fields.BSSID !== "00:00:00:00:00:00")
        manager.connectionInfo.bssid = fields.BSSID;

      if (notifyStateChange(fields) && fields.state === "COMPLETED") {
        onconnected();
      }
      return true;
    }
    if (eventData.indexOf("CTRL-EVENT-DRIVER-STATE") === 0) {
      var handlerName = driverEventMap[eventData];
      if (handlerName)
        notify(handlerName);
      return true;
    }
    if (eventData.indexOf("CTRL-EVENT-TERMINATING") === 0) {
      // If the monitor socket is closed, we have already stopped the
      // supplicant and we can stop waiting for more events and
      // simply exit here (we don't have to notify about having lost
      // the connection).
      if (eventData.indexOf("connection closed") !== -1) {
        notify("supplicantlost", { success: true });
        return false;
      }

      // As long we haven't seen too many recv errors yet, we
      // will keep going for a bit longer.
      if (eventData.indexOf("recv error") !== -1 && ++recvErrors < 10)
        return true;

      notifyStateChange({ state: "DISCONNECTED", BSSID: null, id: -1 });
      notify("supplicantlost", { success: true });
      return false;
    }
    if (eventData.indexOf("CTRL-EVENT-DISCONNECTED") === 0) {
      var token = event.split(" ")[1];
      var bssid = token.split("=")[1];
      if (manager.authenticationFailuresCount > MAX_RETRIES_ON_AUTHENTICATION_FAILURE) {
        manager.authenticationFailuresCount = 0;
        notify("disconnected", {ssid: manager.connectionInfo.ssid});
      }
      manager.connectionInfo.bssid = null;
      manager.connectionInfo.ssid = null;
      manager.connectionInfo.id = -1;
      return true;
    }
    // Association reject is triggered mostly on incorrect WEP key.
    if (eventData.indexOf("CTRL-EVENT-ASSOC-REJECT") === 0) {
      notify("passwordmaybeincorrect");
      if (manager.authenticationFailuresCount > MAX_RETRIES_ON_AUTHENTICATION_FAILURE) {
        manager.authenticationFailuresCount = 0;
        notify("disconnected", {ssid: manager.connectionInfo.ssid});
      }
      return true;
    }
    if (eventData.indexOf("CTRL-EVENT-EAP-FAILURE") === 0) {
      if (event.indexOf("EAP authentication failed") !== -1) {
        notify("passwordmaybeincorrect");
      }
      return true;
    }
    if (eventData.indexOf("CTRL-EVENT-CONNECTED") === 0) {
      // Format: CTRL-EVENT-CONNECTED - Connection to 00:1e:58:ec:d5:6d completed (reauth) [id=1 id_str=]
      var bssid = event.split(" ")[4];

      var keyword = "id=";
      var id = event.substr(event.indexOf(keyword) + keyword.length).split(" ")[0];
      // Read current BSSID here, it will always being provided.
      manager.connectionInfo.id = id;
      manager.connectionInfo.bssid = bssid;
      return true;
    }
    if (eventData.indexOf("CTRL-EVENT-SCAN-RESULTS") === 0) {
      debug("Notifying of scan results available");
      if (reEnableBackgroundScan) {
        reEnableBackgroundScan = false;
        setBackgroundScan("ON", function() {});
      }
      notify("scanresultsavailable");
      return true;
    }
    if (eventData.indexOf("WPS-TIMEOUT") === 0) {
      notifyStateChange({ state: "WPS_TIMEOUT", BSSID: null, id: -1 });
      return true;
    }
    if (eventData.indexOf("WPS-FAIL") === 0) {
      notifyStateChange({ state: "WPS_FAIL", BSSID: null, id: -1 });
      return true;
    }
    if (eventData.indexOf("WPS-OVERLAP-DETECTED") === 0) {
      notifyStateChange({ state: "WPS_OVERLAP_DETECTED", BSSID: null, id: -1 });
      return true;
    }
    // Unknown event.
    return true;
  }

  const SUPP_PROP = "init.svc.wpa_supplicant";
  function killSupplicant(callback) {
    // It is interesting to note that this function does exactly what
    // wifi_stop_supplicant does. Unforunately, on the Galaxy S2, Samsung
    // changed that function in a way that means that it doesn't recognize
    // wpa_supplicant as already running. Therefore, we have to roll our own
    // version here.
    var count = 0;
    var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    function tick() {
      getProperty(SUPP_PROP, "stopped", function (result) {
        if (result === null) {
          callback();
          return;
        }
        if (result === "stopped" || ++count >= 5) {
          // Either we succeeded or ran out of time.
          timer = null;
          callback();
          return;
        }

        // Else it's still running, continue waiting.
        timer.initWithCallback(tick, 1000, Ci.nsITimer.TYPE_ONE_SHOT);
      });
    }

    setProperty("ctl.stop", "wpa_supplicant", tick);
  }

  function didConnectSupplicant(callback) {
    waitForEvent();

    // Load up the supplicant state.
    getDebugEnabled(function(ok) {
      syncDebug();
    });
    statusCommand(function(status) {
      parseStatus(status);
      notify("supplicantconnection");
      callback();
    });
  }

  function prepareForStartup(callback) {
    manager.connectionDropped(function() {
      // Ignore any errors and kill any currently-running supplicants. On some
      // phones, stopSupplicant won't work for a supplicant that we didn't
      // start, so we hand-roll it here.
      suppressEvents = true;
      killSupplicant(function() {
        disableInterface(manager.ifname, function (ok) {
          suppressEvents = false;
          callback();
        });
      });
    });
  }

  // Initial state.
  manager.state = "UNINITIALIZED";
  manager.tetheringState = "UNINITIALIZED";
  manager.enabled = false;
  manager.supplicantStarted = false;
  manager.connectionInfo = { ssid: null, bssid: null, id: -1 };
  manager.authenticationFailuresCount = 0;
  manager.loopDetectionCount = 0;

  const DRIVER_READY_WAIT = 2000;
  var waitForDriverReadyTimer = null;
  function cancelWaitForDriverReadyTimer() {
    if (waitForDriverReadyTimer) {
      waitForDriverReadyTimer.cancel();
      waitForDriverReadyTimer = null;
    }
  };
  function createWaitForDriverReadyTimer(onTimeout) {
    waitForDriverReadyTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    waitForDriverReadyTimer.initWithCallback(onTimeout,
                                             DRIVER_READY_WAIT,
                                             Ci.nsITimer.TYPE_ONE_SHOT);
  };

  // Public interface of the wifi service.
  manager.setWifiEnabled = function(enable, callback) {
    if (enable === manager.enabled) {
      callback("no change");
      return;
    }

    if (enable) {
      manager.state = "INITIALIZING";
      // Kill any existing connections if necessary.
      getProperty("wifi.interface", "tiwlan0", function (ifname) {
        if (!ifname) {
          callback(-1);
          manager.state = "UNINITIALIZED";
          return;
        }
        manager.ifname = ifname;

        // Register as network interface.
        WifiNetworkInterface.name = ifname;
        if (!WifiNetworkInterface.registered) {
          gNetworkManager.registerNetworkInterface(WifiNetworkInterface);
          WifiNetworkInterface.registered = true;
        }
        WifiNetworkInterface.state = Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED;
        WifiNetworkInterface.ip = null;
        WifiNetworkInterface.netmask = null;
        WifiNetworkInterface.broadcast = null;
        WifiNetworkInterface.gateway = null;
        WifiNetworkInterface.dns1 = null;
        WifiNetworkInterface.dns2 = null;
        Services.obs.notifyObservers(WifiNetworkInterface,
                                     kNetworkInterfaceStateChangedTopic,
                                     null);

        prepareForStartup(function() {
          loadDriver(function (status) {
            if (status < 0) {
              callback(status);
              manager.state = "UNINITIALIZED";
              return;
            }

            function doStartSupplicant() {
              cancelWaitForDriverReadyTimer();
              startSupplicant(function (status) {
                if (status < 0) {
                  unloadDriver(function() {
                    callback(status);
                  });
                  manager.state = "UNINITIALIZED";
                  return;
                }

                manager.supplicantStarted = true;
                enableInterface(ifname, function (ok) {
                  callback(ok ? 0 : -1);
                });
              });
            }

            // Driver startup on certain platforms takes longer than it takes for us
            // to return from loadDriver, so wait 2 seconds before starting
            // the supplicant to give it a chance to start.
            createWaitForDriverReadyTimer(doStartSupplicant);
         });
        });
      });
    } else {
      // Note these following calls ignore errors. If we fail to kill the
      // supplicant gracefully, then we need to continue telling it to die
      // until it does.
      terminateSupplicant(function (ok) {
        manager.connectionDropped(function () {
          stopSupplicant(function (status) {
            closeSupplicantConnection(function () {
              manager.state = "UNINITIALIZED";
              disableInterface(manager.ifname, function (ok) {
                unloadDriver(callback);
              });
            });
          });
        });
      });
    }
  }

  // Get wifi interface and load wifi driver when enable Ap mode.
  manager.setWifiApEnabled = function(enabled, configuration, callback) {
    if (enabled) {
      manager.tetheringState = "INITIALIZING";
      getProperty("wifi.interface", "tiwlan0", function (ifname) {
        if (!ifname) {
          callback();
          manager.tetheringState = "UNINITIALIZED";
          return;
        }
        manager.ifname = ifname;
        loadDriver(function (status) {
          if (status < 0) {
            callback();
            manager.tetheringState = "UNINITIALIZED";
            return;
          }

          function doStartWifiTethering() {
            cancelWaitForDriverReadyTimer();
            WifiNetworkInterface.name = manager.ifname;
            gNetworkManager.setWifiTethering(enabled, WifiNetworkInterface,
                                             configuration, function(result) {
              if (result) {
                manager.tetheringState = "UNINITIALIZED";
              } else {
                manager.tetheringState = "COMPLETED";
              }
              // Pop out current request.
              callback();
              // Should we fire a dom event if we fail to set wifi tethering  ?
              debug("Enable Wifi tethering result: " + (result ? result : "successfully"));
            });
          }

          // Driver startup on certain platforms takes longer than it takes
          // for us to return from loadDriver, so wait 2 seconds before
          // turning on Wifi tethering.
          createWaitForDriverReadyTimer(doStartWifiTethering);
        });
      });
    } else {
      gNetworkManager.setWifiTethering(enabled, WifiNetworkInterface,
                                       configuration, function(result) {
        // Should we fire a dom event if we fail to set wifi tethering  ?
        debug("Disable Wifi tethering result: " + (result ? result : "successfully"));
        // Unload wifi driver even if we fail to control wifi tethering.
        unloadDriver(function(status) {
          if (status < 0) {
            debug("Fail to unload wifi driver");
          }
          manager.tetheringState = "UNINITIALIZED";
          callback();
        });
      });
    }
  }

  manager.disconnect = disconnectCommand;
  manager.reconnect = reconnectCommand;
  manager.reassociate = reassociateCommand;

  var networkConfigurationFields = [
    "ssid", "bssid", "psk", "wep_key0", "wep_key1", "wep_key2", "wep_key3",
    "wep_tx_keyidx", "priority", "key_mgmt", "scan_ssid", "disabled",
    "identity", "password", "auth_alg", "phase1", "phase2", "eap"
  ];

  manager.getNetworkConfiguration = function(config, callback) {
    var netId = config.netId;
    var done = 0;
    for (var n = 0; n < networkConfigurationFields.length; ++n) {
      let fieldName = networkConfigurationFields[n];
      getNetworkVariableCommand(netId, fieldName, function(value) {
        if (value !== null)
          config[fieldName] = value;
        if (++done == networkConfigurationFields.length)
          callback(config);
      });
    }
  }
  manager.setNetworkConfiguration = function(config, callback) {
    var netId = config.netId;
    var done = 0;
    var errors = 0;
    for (var n = 0; n < networkConfigurationFields.length; ++n) {
      let fieldName = networkConfigurationFields[n];
      if (!(fieldName in config) ||
          // These fields are special: We can't retrieve them from the
          // supplicant, and often we have a star in our config. In that case,
          // we need to avoid overwriting the correct password with a *.
          (fieldName === "password" ||
           fieldName === "wep_key0" ||
           fieldName === "psk") &&
          config[fieldName] === '*') {
        ++done;
      } else {
        setNetworkVariableCommand(netId, fieldName, config[fieldName], function(ok) {
          if (!ok)
            ++errors;
          if (++done == networkConfigurationFields.length)
            callback(errors == 0);
        });
      }
    }
    // If config didn't contain any of the fields we want, don't lose the error callback.
    if (done == networkConfigurationFields.length)
      callback(false);
  }
  manager.getConfiguredNetworks = function(callback) {
    listNetworksCommand(function (reply) {
      var networks = Object.create(null);
      var lines = reply.split("\n");
      if (lines.length === 1) {
        // We need to make sure we call the callback even if there are no
        // configured networks.
        callback(networks);
        return;
      }

      var done = 0;
      var errors = 0;
      for (var n = 1; n < lines.length; ++n) {
        var result = lines[n].split("\t");
        var netId = result[0];
        var config = networks[netId] = { netId: netId };
        switch (result[3]) {
        case "[CURRENT]":
          config.status = "CURRENT";
          break;
        case "[DISABLED]":
          config.status = "DISABLED";
          break;
        default:
          config.status = "ENABLED";
          break;
        }
        manager.getNetworkConfiguration(config, function (ok) {
            if (!ok)
              ++errors;
            if (++done == lines.length - 1) {
              if (errors) {
                // If an error occured, delete the new netId.
                removeNetworkCommand(netId, function() {
                  callback(null);
                });
              } else {
                callback(networks);
              }
            }
        });
      }
    });
  }
  manager.addNetwork = function(config, callback) {
    addNetworkCommand(function (netId) {
      config.netId = netId;
      manager.setNetworkConfiguration(config, function (ok) {
        if (!ok) {
          removeNetworkCommand(netId, function() { callback(false); });
          return;
        }

        callback(ok);
      });
    });
  }
  manager.updateNetwork = function(config, callback) {
    manager.setNetworkConfiguration(config, callback);
  }
  manager.removeNetwork = function(netId, callback) {
    removeNetworkCommand(netId, callback);
  }

  function ipToString(n) {
    return String((n >>  0) & 0xFF) + "." +
                 ((n >>  8) & 0xFF) + "." +
                 ((n >> 16) & 0xFF) + "." +
                 ((n >> 24) & 0xFF);
  }

  function stringToIp(string) {
    let ip = 0;
    let start, end = -1;
    for (let i = 0; i < 4; i++) {
      start = end + 1;
      end = string.indexOf(".", start);
      if (end == -1) {
        end = string.length;
      }
      let num = parseInt(string.slice(start, end), 10);
      if (isNaN(num)) {
        return 0;
      }
      ip |= num << (i * 8);
    }
    return ip;
  }

  function swap32(n) {
    return (((n >> 24) & 0xFF) <<  0) |
           (((n >> 16) & 0xFF) <<  8) |
           (((n >>  8) & 0xFF) << 16) |
           (((n >>  0) & 0xFF) << 24);
  }

  function ntohl(n) {
    return swap32(n);
  }

  function makeMask(len) {
    let mask = 0;
    for (let i = 0; i < len; ++i) {
      mask |= (0x80000000 >> i);
    }
    return ntohl(mask);
  }

  manager.saveConfig = function(callback) {
    saveConfigCommand(callback);
  }
  manager.enableNetwork = function(netId, disableOthers, callback) {
    enableNetworkCommand(netId, disableOthers, callback);
  }
  manager.disableNetwork = function(netId, callback) {
    disableNetworkCommand(netId, callback);
  }
  manager.getMacAddress = getMacAddressCommand;
  manager.getScanResults = scanResultsCommand;
  manager.setScanMode = function(mode, callback) {
    setScanModeCommand(mode === "active", callback);
  }
  manager.setBackgroundScan = setBackgroundScan;
  manager.scan = scanCommand;
  manager.wpsPbc = wpsPbcCommand;
  manager.wpsPin = wpsPinCommand;
  manager.wpsCancel = wpsCancelCommand;
  manager.setPowerMode = (sdkVersion >= 16)
                         ? setPowerModeCommandJB
                         : setPowerModeCommandICS;
  manager.setSuspendOptimizations = setSuspendOptimizationsCommand;
  manager.setStaticIpMode = setStaticIpMode;
  manager.getRssiApprox = getRssiApproxCommand;
  manager.getLinkSpeed = getLinkSpeedCommand;
  manager.getDhcpInfo = function() { return dhcpInfo; }
  manager.getConnectionInfo = (sdkVersion >= 15)
                              ? getConnectionInfoICS
                              : getConnectionInfoGB;

  manager.isHandShakeState = function(state) {
    switch (state) {
      case "AUTHENTICATING":
      case "ASSOCIATING":
      case "ASSOCIATED":
      case "FOUR_WAY_HANDSHAKE":
      case "GROUP_HANDSHAKE":
        return true;
      case "DORMANT":
      case "COMPLETED":
      case "DISCONNECTED":
      case "INTERFACE_DISABLED":
      case "INACTIVE":
      case "SCANNING":
      case "UNINITIALIZED":
      case "INVALID":
      case "CONNECTED":
      default:
        return false;
    }
  }
  manager.syncDebug = syncDebug;
  manager.stateOrdinal = function(state) {
    return supplicantStatesMap.indexOf(state);
  }
  manager.supplicantLoopDetection = function(prevState, state) {
    var isPrevStateInHandShake = manager.isHandShakeState(prevState);
    var isStateInHandShake = manager.isHandShakeState(state);

    if (isPrevStateInHandShake) {
      if (isStateInHandShake) {
        // Increase the count only if we are in the loop.
        if (manager.stateOrdinal(state) > manager.stateOrdinal(prevState)) {
          manager.loopDetectionCount++;
        }
        if (manager.loopDetectionCount > MAX_SUPPLICANT_LOOP_ITERATIONS) {
          notify("disconnected", {ssid: manager.connectionInfo.ssid});
          manager.loopDetectionCount = 0;
        }
      }
    } else {
      // From others state to HandShake state. Reset the count.
      if (isStateInHandShake) {
        manager.loopDetectionCount = 0;
      }
    }
  }

  return manager;
})();

// Get unique key for a network, now the key is created by escape(SSID)+Security.
// So networks of same SSID but different security mode can be identified.
function getNetworkKey(network)
{
  var ssid = "",
      encryption = "OPEN";

  if ("security" in network) {
    // manager network object, represents an AP
    // object structure
    // {
    //   .ssid           : SSID of AP
    //   .security[]     : "WPA-PSK" for WPA-PSK
    //                     "WPA-EAP" for WPA-EAP
    //                     "WEP" for WEP
    //                     "" for OPEN
    //   other keys
    // }

    var security = network.security;
    ssid = network.ssid;

    for (let j = 0; j < security.length; j++) {
      if (security[j] === "WPA-PSK") {
        encryption = "WPA-PSK";
        break;
      } else if (security[j] === "WPA-EAP") {
        encryption = "WPA-EAP";
        break;
      } else if (security[j] === "WEP") {
        encryption = "WEP";
        break;
      }
    }
  } else if ("key_mgmt" in network) {
    // configure network object, represents a network
    // object structure
    // {
    //   .ssid           : SSID of network, quoted
    //   .key_mgmt       : Encryption type
    //                     "WPA-PSK" for WPA-PSK
    //                     "WPA-EAP" for WPA-EAP
    //                     "NONE" for WEP/OPEN
    //   .auth_alg       : Encryption algorithm(WEP mode only)
    //                     "OPEN_SHARED" for WEP
    //   other keys
    // }
    var key_mgmt = network.key_mgmt,
        auth_alg = network.auth_alg;
    ssid = dequote(network.ssid);

    if (key_mgmt == "WPA-PSK") {
      encryption = "WPA-PSK";
    } else if (key_mgmt == "WPA-EAP") {
      encryption = "WPA-EAP";
    } else if (key_mgmt == "NONE" && auth_alg === "OPEN SHARED") {
      encryption = "WEP";
    }
  }

  // ssid here must be dequoted, and it's safer to esacpe it.
  // encryption won't be empty and always be assigned one of the followings :
  // "OPEN"/"WEP"/"WPA-PSK"/"WPA-EAP".
  // So for a invalid network object, the returned key will be "OPEN".
  return escape(ssid) + encryption;
}

function getKeyManagement(flags) {
  var types = [];
  if (!flags)
    return types;

  if (/\[WPA2?-PSK/.test(flags))
    types.push("WPA-PSK");
  if (/\[WPA2?-EAP/.test(flags))
    types.push("WPA-EAP");
  if (/\[WEP/.test(flags))
    types.push("WEP");
  return types;
}

function getCapabilities(flags) {
  var types = [];
  if (!flags)
    return types;

  if (/\[WPS/.test(flags))
    types.push("WPS");
  return types;
}

// These constants shamelessly ripped from WifiManager.java
// strength is the value returned by scan_results. It is nominally in dB. We
// transform it into a percentage for clients looking to simply show a
// relative indication of the strength of a network.
const MIN_RSSI = -100;
const MAX_RSSI = -55;

function calculateSignal(strength) {
  // Some wifi drivers represent their signal strengths as 8-bit integers, so
  // in order to avoid negative numbers, they add 256 to the actual values.
  // While we don't *know* that this is the case here, we make an educated
  // guess.
  if (strength > 0)
    strength -= 256;

  if (strength <= MIN_RSSI)
    return 0;
  if (strength >= MAX_RSSI)
    return 100;
  return Math.floor(((strength - MIN_RSSI) / (MAX_RSSI - MIN_RSSI)) * 100);
}

function Network(ssid, security, password, capabilities) {
  this.ssid = ssid;
  this.security = security;

  if (typeof password !== "undefined")
    this.password = password;
  if (capabilities !== undefined)
    this.capabilities = capabilities;
  // TODO connected here as well?

  this.__exposedProps__ = Network.api;
}

Network.api = {
  ssid: "r",
  security: "r",
  capabilities: "r",
  known: "r",

  password: "rw",
  keyManagement: "rw",
  psk: "rw",
  identity: "rw",
  wep: "rw",
  hidden: "rw",
  eap: "rw",
  phase1: "rw",
  phase2: "rw"
};

// Note: We never use ScanResult.prototype, so the fact that it's unrelated to
// Network.prototype is OK.
function ScanResult(ssid, bssid, flags, signal) {
  Network.call(this, ssid, getKeyManagement(flags), undefined,
               getCapabilities(flags));
  this.bssid = bssid;
  this.signalStrength = signal;
  this.relSignalStrength = calculateSignal(Number(signal));

  this.__exposedProps__ = ScanResult.api;
}

// XXX This should probably live in the DOM-facing side, but it's hard to do
// there, so we stick this here.
ScanResult.api = {
  bssid: "r",
  signalStrength: "r",
  relSignalStrength: "r",
  connected: "r"
};

for (let i in Network.api) {
  ScanResult.api[i] = Network.api[i];
}

function quote(s) {
  return '"' + s + '"';
}

function dequote(s) {
  if (s[0] != '"' || s[s.length - 1] != '"')
    throw "Invalid argument, not a quoted string: " + s;
  return s.substr(1, s.length - 2);
}

function isWepHexKey(s) {
  if (s.length != 10 && s.length != 26 && s.length != 58)
    return false;
  return !/[^a-fA-F0-9]/.test(s);
}


let WifiNetworkInterface = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInterface]),

  registered: false,

  // nsINetworkInterface

  NETWORK_STATE_UNKNOWN:       Ci.nsINetworkInterface.NETWORK_STATE_UNKNOWN,
  NETWORK_STATE_CONNECTING:    Ci.nsINetworkInterface.CONNECTING,
  NETWORK_STATE_CONNECTED:     Ci.nsINetworkInterface.CONNECTED,
  NETWORK_STATE_DISCONNECTING: Ci.nsINetworkInterface.DISCONNECTING,
  NETWORK_STATE_DISCONNECTED:  Ci.nsINetworkInterface.DISCONNECTED,

  state: Ci.nsINetworkInterface.NETWORK_STATE_UNKNOWN,

  NETWORK_TYPE_WIFI:        Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
  NETWORK_TYPE_MOBILE:      Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,
  NETWORK_TYPE_MOBILE_MMS:  Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS,
  NETWORK_TYPE_MOBILE_SUPL: Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL,

  type: Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,

  name: null,

  // For now we do our own DHCP. In the future this should be handed off
  // to the Network Manager.
  dhcp: false,

  ip: null,

  netmask: null,

  broadcast: null,

  dns1: null,

  dns2: null,

  httpProxyHost: null,

  httpProxyPort: null,

};

function WifiScanResult() {}

// TODO Make the difference between a DOM-based network object and our
// networks objects much clearer.
let netToDOM;
let netFromDOM;

function WifiWorker() {
  var self = this;

  this._mm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
               .getService(Ci.nsIMessageListenerManager);
  const messages = ["WifiManager:getNetworks", "WifiManager:getKnownNetworks",
                    "WifiManager:associate", "WifiManager:forget",
                    "WifiManager:wps", "WifiManager:getState",
                    "WifiManager:setPowerSavingMode",
                    "WifiManager:setStaticIpMode",
                    "child-process-shutdown"];

  messages.forEach((function(msgName) {
    this._mm.addMessageListener(msgName, this);
  }).bind(this));

  Services.obs.addObserver(this, kMozSettingsChangedObserverTopic, false);

  this.wantScanResults = [];

  this._allowWpaEap = false;
  this._needToEnableNetworks = false;
  this._highestPriority = -1;

  // Networks is a map from SSID -> a scan result.
  this.networks = Object.create(null);

  // ConfiguredNetworks is a map from SSID -> our view of a network. It only
  // lists networks known to the wpa_supplicant. The SSID field (and other
  // fields) are quoted for ease of use with WifiManager commands.
  // Note that we don't have to worry about escaping embedded quotes since in
  // all cases, the supplicant will take the last quotation that we pass it as
  // the end of the string.
  this.configuredNetworks = Object.create(null);
  this._addingNetworks = Object.create(null);

  this.currentNetwork = null;
  this.ipAddress = "";

  this._lastConnectionInfo = null;
  this._connectionInfoTimer = null;
  this._reconnectOnDisconnect = false;

  // Users of instances of nsITimer should keep a reference to the timer until
  // it is no longer needed in order to assure the timer is fired.
  this._callbackTimer = null;

  // XXX On some phones (Otoro and Unagi) the wifi driver doesn't play nicely
  // with the automatic scans that wpa_supplicant does (it appears that the
  // driver forgets that it's returned scan results and then refuses to try to
  // rescan. In order to detect this case we start a timer when we enter the
  // SCANNING state and reset it whenever we either get scan results or leave
  // the SCANNING state. If the timer fires, we assume that we are stuck and
  // forceably try to unstick the supplican, also turning on background
  // scanning to avoid having to constantly poke the supplicant.

  // How long we wait is controlled by the SCAN_STUCK_WAIT constant.
  const SCAN_STUCK_WAIT = 12000;
  this._scanStuckTimer = null;
  this._turnOnBackgroundScan = false;

  function startScanStuckTimer() {
    if (WifiManager.schedScanRecovery) {
      self._scanStuckTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      self._scanStuckTimer.initWithCallback(scanIsStuck, SCAN_STUCK_WAIT,
                                            Ci.nsITimer.TYPE_ONE_SHOT);
    }
  }

  function scanIsStuck() {
    // Uh-oh, we've waited too long for scan results. Disconnect (which
    // guarantees that we leave the SCANNING state and tells wpa_supplicant to
    // wait for our next command) ensure that background scanning is on and
    // then try again.
    debug("Determined that scanning is stuck, turning on background scanning!");
    WifiManager.disconnect(function(ok) {});
    self._turnOnBackgroundScan = true;
  }

  // A list of requests to turn wifi on or off.
  this._stateRequests = [];

  // Given a connection status network, takes a network from
  // self.configuredNetworks and prepares it for the DOM.
  netToDOM = function(net) {
    var ssid = dequote(net.ssid);
    var security = (net.key_mgmt === "NONE" && net.wep_key0) ? ["WEP"] :
                   (net.key_mgmt && net.key_mgmt !== "NONE") ? [net.key_mgmt] :
                   [];
    var password;
    if (("psk" in net && net.psk) ||
        ("password" in net && net.password) ||
        ("wep_key0" in net && net.wep_key0)) {
      password = "*";
    }

    var pub = new Network(ssid, security, password);
    if (net.identity)
      pub.identity = dequote(net.identity);
    if (net.netId)
      pub.known = true;
    if (net.scan_ssid === 1)
      pub.hidden = true;
    return pub;
  };

  netFromDOM = function(net, configured) {
    // Takes a network from the DOM and makes it suitable for insertion into
    // self.configuredNetworks (that is calling addNetwork will do the right
    // thing).
    // NB: Modifies net in place: safe since we don't share objects between
    // the dom and the chrome code.

    // Things that are useful for the UI but not to us.
    delete net.bssid;
    delete net.signalStrength;
    delete net.relSignalStrength;
    delete net.security;
    delete net.capabilities;

    if (!configured)
      configured = {};

    net.ssid = quote(net.ssid);

    let wep = false;
    if ("keyManagement" in net) {
      if (net.keyManagement === "WEP") {
        wep = true;
        net.keyManagement = "NONE";
      }

      configured.key_mgmt = net.key_mgmt = net.keyManagement; // WPA2-PSK, WPA-PSK, etc.
      delete net.keyManagement;
    } else {
      configured.key_mgmt = net.key_mgmt = "NONE";
    }

    if (net.hidden) {
      configured.scan_ssid = net.scan_ssid = 1;
      delete net.hidden;
    }

    function checkAssign(name, checkStar) {
      if (name in net) {
        let value = net[name];
        if (!value || (checkStar && value === '*')) {
          if (name in configured)
            net[name] = configured[name];
          else
            delete net[name];
        } else {
          configured[name] = net[name] = quote(value);
        }
      }
    }

    checkAssign("psk", true);
    checkAssign("identity", false);
    checkAssign("password", true);
    if (wep && net.wep && net.wep != '*') {
      configured.wep_key0 = net.wep_key0 = isWepHexKey(net.wep) ? net.wep : quote(net.wep);
      configured.auth_alg = net.auth_alg = "OPEN SHARED";
    }

    if ("phase1" in net)
      net.phase1 = quote(net.phase1);

    if ("phase2" in net)
      net.phase2 = quote(net.phase2);

    return net;
  };

  WifiManager.onsupplicantconnection = function() {
    debug("Connected to supplicant");
    WifiManager.enabled = true;
    self._reloadConfiguredNetworks(function(ok) {
      // Prime this.networks.
      if (!ok)
        return;

      self.waitForScan(function firstScan() {});
      // The select network command we used in associate() disables others networks.
      // Enable them here to make sure wpa_supplicant helps to connect to known
      // network automatically.
      self._enableAllNetworks();
      WifiManager.saveConfig(function() {})
    });

    try {
      self._allowWpaEap = Services.prefs.getBoolPref("b2g.wifi.allow_unsafe_wpa_eap");
    } catch (e) {
      self._allowWpaEap = false;
    }

    // Check if we need to dequeue requests first.
    self._notifyAfterStateChange(true, true);

    // Notify everybody, even if they didn't ask us to come up.
    WifiManager.getMacAddress(function (mac) {
      self.macAddress = mac;
      debug("Got mac: " + mac);
      self._fireEvent("wifiUp", { macAddress: mac });
    });

    if (WifiManager.state === "SCANNING")
      startScanStuckTimer();
  };

  WifiManager.onsupplicantlost = function() {
    WifiManager.enabled = WifiManager.supplicantStarted = false;
    WifiManager.state = "UNINITIALIZED";
    debug("Supplicant died!");

    // Check if we need to dequeue requests first.
    self._notifyAfterStateChange(this.success, false);

    // Notify everybody, even if they didn't ask us to come up.
    self._fireEvent("wifiDown", {});
  };

  WifiManager.onpasswordmaybeincorrect = function() {
    WifiManager.authenticationFailuresCount++;
  };

  WifiManager.ondisconnected = function() {
    // We may fail to establish the connection, re-enable the
    // rest of our networks.
    if (self._needToEnableNetworks) {
      self._enableAllNetworks();
      self._needToEnableNetworks = false;
    }

    WifiManager.getCurrentNetworkId(this.ssid, function(netId) {
      // Trying to get netId from current network.
      if (!netId &&
          self.currentNetwork &&
          typeof self.currentNetwork.netId !== "undefined") {
        netId = self.currentNetwork.netId;
      }
      if (netId) {
        WifiManager.disableNetwork(netId, function() {});
      }
    });
    self._fireEvent("onconnectingfailed", {network: self.currentNetwork});
  }

  WifiManager.onstatechange = function() {
    debug("State change: " + this.prevState + " -> " + this.state);

    if (self._connectionInfoTimer &&
        this.state !== "CONNECTED" &&
        this.state !== "COMPLETED") {
      self._stopConnectionInfoTimer();
    }

    if (this.state !== "SCANNING" &&
        self._scanStuckTimer) {
      self._scanStuckTimer.cancel();
      self._scanStuckTimer = null;
    }

    switch (this.state) {
      case "DORMANT":
        // The dormant state is a bad state to be in since we won't
        // automatically connect. Try to knock us out of it. We only
        // hit this state when we've failed to run DHCP, so trying
        // again isn't the worst thing we can do. Eventually, we'll
        // need to detect if we're looping in this state and bail out.
        WifiManager.reconnect(function(){});
        break;
      case "ASSOCIATING":
        // id has not yet been filled in, so we can only report the ssid and
        // bssid.
        self.currentNetwork =
          { bssid: WifiManager.connectionInfo.bssid,
            ssid: quote(WifiManager.connectionInfo.ssid) };
        self._fireEvent("onconnecting", { network: netToDOM(self.currentNetwork) });
        break;
      case "ASSOCIATED":
        if (!self.currentNetwork) {
          self.currentNetwork =
            { bssid: WifiManager.connectionInfo.bssid,
              ssid: quote(WifiManager.connectionInfo.ssid) };
        }

        self.currentNetwork.netId = this.id;
        WifiManager.getNetworkConfiguration(self.currentNetwork, function (){});
        break;
      case "COMPLETED":
        // Now that we've successfully completed the connection, re-enable the
        // rest of our networks.
        // XXX Need to do this eventually if the user entered an incorrect
        // password. For now, we require user interaction to break the loop and
        // select a better network!
        if (self._needToEnableNetworks) {
          self._enableAllNetworks();
          self._needToEnableNetworks = false;
        }

        // We get the ASSOCIATED event when we've associated but not connected, so
        // wait until the handshake is complete.
        if (this.fromStatus || !self.currentNetwork) {
          // In this case, we connected to an already-connected wpa_supplicant,
          // because of that we need to gather information about the current
          // network here.
          self.currentNetwork = { ssid: quote(WifiManager.connectionInfo.ssid),
                                  netId: WifiManager.connectionInfo.id };
          WifiManager.getNetworkConfiguration(self.currentNetwork, function(){});
        }

        // The full authentication process is completed, reset the count.
        WifiManager.authenticationFailuresCount = 0;
        WifiManager.loopDetectionCount = 0;
        self._startConnectionInfoTimer();
        self._fireEvent("onassociate", { network: netToDOM(self.currentNetwork) });
        break;
      case "CONNECTED":
        // BSSID is read after connected, update it.
        self.currentNetwork.bssid = WifiManager.connectionInfo.bssid;
        break;
      case "DISCONNECTED":
        self._fireEvent("ondisconnect", {});
        self.currentNetwork = null;
        self.ipAddress = "";

        if (self._turnOnBackgroundScan) {
          self._turnOnBackgroundScan = false;
          WifiManager.setBackgroundScan("ON", function(did_something, ok) {
            WifiManager.reassociate(function() {});
          });
        }

        WifiManager.connectionDropped(function() {
          // We've disconnected from a network because of a call to forgetNetwork.
          // Reconnect to the next available network (if any).
          if (self._reconnectOnDisconnect) {
            self._reconnectOnDisconnect = false;
            WifiManager.reconnect(function(){});
          }
        });

        WifiNetworkInterface.state =
          Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED;
        WifiNetworkInterface.ip = null;
        WifiNetworkInterface.netmask = null;
        WifiNetworkInterface.broadcast = null;
        WifiNetworkInterface.gateway = null;
        WifiNetworkInterface.dns1 = null;
        WifiNetworkInterface.dns2 = null;
        Services.obs.notifyObservers(WifiNetworkInterface,
                                     kNetworkInterfaceStateChangedTopic,
                                     null);

        break;
      case "WPS_TIMEOUT":
        self._fireEvent("onwpstimeout", {});
        break;
      case "WPS_FAIL":
        self._fireEvent("onwpsfail", {});
        break;
      case "WPS_OVERLAP_DETECTED":
        self._fireEvent("onwpsoverlap", {});
        break;
      case "SCANNING":
        // If we're already scanning in the background, we don't need to worry
        // about getting stuck while scanning.
        if (!WifiManager.backgroundScanEnabled && WifiManager.enabled)
          startScanStuckTimer();
        break;
    }
  };

  WifiManager.onnetworkconnected = function() {
    if (this.info) {
      WifiNetworkInterface.state =
        Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED;
      WifiNetworkInterface.ip = this.info.ipaddr_str;
      WifiNetworkInterface.netmask = this.info.mask_str;
      WifiNetworkInterface.broadcast = this.info.broadcast_str;
      WifiNetworkInterface.gateway = this.info.gateway_str;
      WifiNetworkInterface.dns1 = this.info.dns1_str;
      WifiNetworkInterface.dns2 = this.info.dns2_str;
      Services.obs.notifyObservers(WifiNetworkInterface,
                                   kNetworkInterfaceStateChangedTopic,
                                   null);

      self.ipAddress = this.info.ipaddr_str;

      // We start the connection information timer when we associate, but
      // don't have our IP address until here. Make sure that we fire a new
      // connectionInformation event with the IP address the next time the
      // timer fires.
      self._lastConnectionInfo = null;
      self._fireEvent("onconnect", { network: netToDOM(self.currentNetwork) });
    } else {
      // NB: We have to call disconnect first. Otherwise, we only reauth with
      // the existing AP and don't retrigger DHCP.
      WifiManager.disconnect(function() {
        WifiManager.reassociate(function(){});
      });
    }
  };

  WifiManager.onscanresultsavailable = function() {
    if (self._scanStuckTimer) {
      // We got scan results! We must not be stuck for now, try again.
      self._scanStuckTimer.cancel();
      self._scanStuckTimer.initWithCallback(scanIsStuck, SCAN_STUCK_WAIT,
                                            Ci.nsITimer.TYPE_ONE_SHOT);
    }

    if (self.wantScanResults.length === 0) {
      debug("Scan results available, but we don't need them");
      return;
    }

    debug("Scan results are available! Asking for them.");
    WifiManager.getScanResults(function(r) {
      // Failure.
      if (!r) {
        self.wantScanResults.forEach(function(callback) { callback(null) });
        self.wantScanResults = [];
        return;
      }

      // Now that we have scan results, there's no more need to continue
      // scanning. Ignore any errors from this command.
      WifiManager.setScanMode("inactive", function() {});
      let lines = r.split("\n");
      // NB: Skip the header line.
      self.networksArray = [];
      for (let i = 1; i < lines.length; ++i) {
        // bssid / frequency / signal level / flags / ssid
        var match = /([\S]+)\s+([\S]+)\s+([\S]+)\s+(\[[\S]+\])?\s+(.*)/.exec(lines[i]);

        if (match && match[5]) {
          let ssid = match[5],
              bssid = match[1],
              signalLevel = match[3],
              flags = match[4];

          // Skip ad-hoc networks which aren't supported (bug 811635).
          if (flags.indexOf("[IBSS]") >= 0)
            continue;

          // If this is the first time that we've seen this SSID in the scan
          // results, add it to the list along with any other information.
          // Also, we use the highest signal strength that we see.
          let network = new ScanResult(ssid, bssid, flags, signalLevel);

          let networkKey = getNetworkKey(network);
          let eapIndex = -1;
          if (networkKey in self.configuredNetworks) {
            let known = self.configuredNetworks[networkKey];
            network.known = true;

            if ("identity" in known && known.identity)
              network.identity = dequote(known.identity);

            // Note: we don't hand out passwords here! The * marks that there
            // is a password that we're hiding.
            if (("psk" in known && known.psk) ||
                ("password" in known && known.password) ||
                ("wep_key0" in known && known.wep_key0)) {
              network.password = "*";
            }
          } else if (!self._allowWpaEap &&
                     (eapIndex = network.security.indexOf("WPA-EAP")) >= 0) {
            // Don't offer to connect to WPA-EAP networks unless one has been
            // configured through other means (e.g. it was added directly to
            // wpa_supplicant.conf). Here, we have an unknown WPA-EAP network,
            // so we ignore it entirely if it only supports WPA-EAP, otherwise
            // we take EAP out of the list and offer the rest of the
            // security.
            if (network.security.length === 1)
              continue;

            network.security.splice(eapIndex, 1);
          }

          self.networksArray.push(network);
          if (network.bssid === WifiManager.connectionInfo.bssid)
            network.connected = true;

          let signal = calculateSignal(Number(match[3]));
          if (signal > network.relSignalStrength)
            network.relSignalStrength = signal;
        } else if (!match) {
          debug("Match didn't find anything for: " + lines[i]);
        }
      }

      self.wantScanResults.forEach(function(callback) { callback(self.networksArray) });
      self.wantScanResults = [];
    });
  };

  // Read the 'wifi.enabled' setting in order to start with a known
  // value at boot time. The handle() will be called after reading.
  //
  // nsISettingsServiceCallback implementation.
  var initWifiEnabledCb = {
    handle: function handle(aName, aResult) {
      if (aName !== SETTINGS_WIFI_ENABLED)
        return;
      if (aResult === null)
        aResult = true;
      self.handleWifiEnabled(aResult);
    },
    handleError: function handleError(aErrorMessage) {
      debug("Error reading the 'wifi.enabled' setting. Default to wifi on.");
      self.handleWifiEnabled(true);
    }
  };

  var initWifiDebuggingEnabledCb = {
    handle: function handle(aName, aResult) {
      if (aName !== SETTINGS_WIFI_DEBUG_ENABLED)
        return;
      if (aResult === null)
        aResult = false;
      DEBUG = aResult;
      updateDebug();
    },
    handleError: function handleError(aErrorMessage) {
      debug("Error reading the 'wifi.debugging.enabled' setting. Default to debugging off.");
      DEBUG = false;
      updateDebug();
    }
  };

  this.initTetheringSettings();

  let lock = gSettingsService.createLock();
  lock.get(SETTINGS_WIFI_ENABLED, initWifiEnabledCb);
  lock.get(SETTINGS_WIFI_DEBUG_ENABLED, initWifiDebuggingEnabledCb);

  lock.get(SETTINGS_WIFI_SSID, this);
  lock.get(SETTINGS_WIFI_SECURITY_TYPE, this);
  lock.get(SETTINGS_WIFI_SECURITY_PASSWORD, this);
  lock.get(SETTINGS_WIFI_IP, this);
  lock.get(SETTINGS_WIFI_PREFIX, this);
  lock.get(SETTINGS_WIFI_DHCPSERVER_STARTIP, this);
  lock.get(SETTINGS_WIFI_DHCPSERVER_ENDIP, this);
  lock.get(SETTINGS_WIFI_DNS1, this);
  lock.get(SETTINGS_WIFI_DNS2, this);
  lock.get(SETTINGS_WIFI_TETHERING_ENABLED, this);

  lock.get(SETTINGS_USB_DHCPSERVER_STARTIP, this);
  lock.get(SETTINGS_USB_DHCPSERVER_ENDIP, this);

  this._wifiTetheringSettingsToRead = [SETTINGS_WIFI_SSID,
                                       SETTINGS_WIFI_SECURITY_TYPE,
                                       SETTINGS_WIFI_SECURITY_PASSWORD,
                                       SETTINGS_WIFI_IP,
                                       SETTINGS_WIFI_PREFIX,
                                       SETTINGS_WIFI_DHCPSERVER_STARTIP,
                                       SETTINGS_WIFI_DHCPSERVER_ENDIP,
                                       SETTINGS_WIFI_DNS1,
                                       SETTINGS_WIFI_DNS2,
                                       SETTINGS_WIFI_TETHERING_ENABLED,
                                       SETTINGS_USB_DHCPSERVER_STARTIP,
                                       SETTINGS_USB_DHCPSERVER_ENDIP];
}

function translateState(state) {
  switch (state) {
    case "INTERFACE_DISABLED":
    case "INACTIVE":
    case "SCANNING":
    case "DISCONNECTED":
    default:
      return "disconnected";

    case "AUTHENTICATING":
    case "ASSOCIATING":
    case "ASSOCIATED":
    case "FOUR_WAY_HANDSHAKE":
    case "GROUP_HANDSHAKE":
      return "connecting";

    case "COMPLETED":
      return WifiManager.getDhcpInfo() ? "connected" : "associated";
  }
}

WifiWorker.prototype = {
  classID:   WIFIWORKER_CID,
  classInfo: XPCOMUtils.generateCI({classID: WIFIWORKER_CID,
                                    contractID: WIFIWORKER_CONTRACTID,
                                    classDescription: "WifiWorker",
                                    interfaces: [Ci.nsIWorkerHolder,
                                                 Ci.nsIWifi,
                                                 Ci.nsIObserver]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWorkerHolder,
                                         Ci.nsIWifi,
                                         Ci.nsISettingsServiceCallback]),

  disconnectedByWifi: false,

  disconnectedByWifiTethering: false,

  _wifiTetheringSettingsToRead: [],

  _oldWifiTetheringEnabledState: null,

  tetheringSettings: {},

  initTetheringSettings: function initTetheringSettings() {
    this.tetheringSettings[SETTINGS_WIFI_TETHERING_ENABLED] = null;
    this.tetheringSettings[SETTINGS_WIFI_SSID] = DEFAULT_WIFI_SSID;
    this.tetheringSettings[SETTINGS_WIFI_SECURITY_TYPE] = DEFAULT_WIFI_SECURITY_TYPE;
    this.tetheringSettings[SETTINGS_WIFI_SECURITY_PASSWORD] = DEFAULT_WIFI_SECURITY_PASSWORD;
    this.tetheringSettings[SETTINGS_WIFI_IP] = DEFAULT_WIFI_IP;
    this.tetheringSettings[SETTINGS_WIFI_PREFIX] = DEFAULT_WIFI_PREFIX;
    this.tetheringSettings[SETTINGS_WIFI_DHCPSERVER_STARTIP] = DEFAULT_WIFI_DHCPSERVER_STARTIP;
    this.tetheringSettings[SETTINGS_WIFI_DHCPSERVER_ENDIP] = DEFAULT_WIFI_DHCPSERVER_ENDIP;
    this.tetheringSettings[SETTINGS_WIFI_DNS1] = DEFAULT_DNS1;
    this.tetheringSettings[SETTINGS_WIFI_DNS2] = DEFAULT_DNS2;

    this.tetheringSettings[SETTINGS_USB_DHCPSERVER_STARTIP] = DEFAULT_USB_DHCPSERVER_STARTIP;
    this.tetheringSettings[SETTINGS_USB_DHCPSERVER_ENDIP] = DEFAULT_USB_DHCPSERVER_ENDIP;
  },

  // Internal methods.
  waitForScan: function(callback) {
    this.wantScanResults.push(callback);
  },

  // In order to select a specific network, we disable the rest of the
  // networks known to us. However, in general, we want the supplicant to
  // connect to which ever network it thinks is best, so when we select the
  // proper network (or fail to), we need to re-enable the rest.
  _enableAllNetworks: function() {
    for each (let net in this.configuredNetworks) {
      WifiManager.enableNetwork(net.netId, false, function(ok) {
        net.disabled = ok ? 1 : 0;
      });
    }
  },

  _startConnectionInfoTimer: function() {
    if (this._connectionInfoTimer)
      return;

    var self = this;
    function getConnectionInformation() {
      WifiManager.getConnectionInfo(function(connInfo) {
        // See comments in calculateSignal for information about this.
        if (!connInfo) {
          self._lastConnectionInfo = null;
          return;
        }

        let { rssi, linkspeed } = connInfo;
        if (rssi > 0)
          rssi -= 256;
        if (rssi <= MIN_RSSI)
          rssi = MIN_RSSI;
        else if (rssi >= MAX_RSSI)
          rssi = MAX_RSSI;

        let info = { signalStrength: rssi,
                     relSignalStrength: calculateSignal(rssi),
                     linkSpeed: linkspeed,
                     ipAddress: self.ipAddress };
        let last = self._lastConnectionInfo;

        // Only fire the event if the link speed changed or the signal
        // strength changed by more than 10%.
        function tensPlace(percent) ((percent / 10) | 0)

        if (last && last.linkSpeed === info.linkSpeed &&
            tensPlace(last.relSignalStrength) === tensPlace(info.relSignalStrength)) {
          return;
        }

        self._lastConnectionInfo = info;
        debug("Firing connectionInfoUpdate: " + uneval(info));
        self._fireEvent("connectionInfoUpdate", info);
      });
    }

    // Prime our _lastConnectionInfo immediately and fire the event at the
    // same time.
    getConnectionInformation();

    // Now, set up the timer for regular updates.
    this._connectionInfoTimer =
      Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._connectionInfoTimer.init(getConnectionInformation, 5000,
                                   Ci.nsITimer.TYPE_REPEATING_SLACK);
  },

  _stopConnectionInfoTimer: function() {
    if (!this._connectionInfoTimer)
      return;

    this._connectionInfoTimer.cancel();
    this._connectionInfoTimer = null;
    this._lastConnectionInfo = null;
  },

  _reloadConfiguredNetworks: function(callback) {
    WifiManager.getConfiguredNetworks((function(networks) {
      if (!networks) {
        debug("Unable to get configured networks");
        callback(false);
        return;
      }

      this._highestPriority = -1;

      // Convert between netId-based and ssid-based indexing.
      for (let net in networks) {
        let network = networks[net];
        if (!network.ssid) {
          delete networks[net]; // TODO support these?
          continue;
        }

        if (network.priority && network.priority > this._highestPriority)
          this._highestPriority = network.priority;

        let networkKey = getNetworkKey(network);
        networks[networkKey] = network;
        delete networks[net];
      }

      this.configuredNetworks = networks;
      callback(true);
    }).bind(this));
  },

  // Important side effect: calls WifiManager.saveConfig.
  _reprioritizeNetworks: function(callback) {
    // First, sort the networks in orer of their priority.
    var ordered = Object.getOwnPropertyNames(this.configuredNetworks);
    let self = this;
    ordered.sort(function(a, b) {
      var neta = self.configuredNetworks[a],
          netb = self.configuredNetworks[b];

      // Sort unsorted networks to the end of the list.
      if (isNaN(neta.priority))
        return isNaN(netb.priority) ? 0 : 1;
      if (isNaN(netb.priority))
        return -1;
      return netb.priority - neta.priority;
    });

    // Skip unsorted networks.
    let newPriority = 0, i;
    for (i = ordered.length - 1; i >= 0; --i) {
      if (!isNaN(this.configuredNetworks[ordered[i]].priority))
        break;
    }

    // No networks we care about?
    if (i < 0) {
      WifiManager.saveConfig(callback);
      return;
    }

    // Now assign priorities from 0 to length, starting with the smallest
    // priority and heading towards the highest (note the dependency between
    // total and i here).
    let done = 0, errors = 0, total = i + 1;
    for (; i >= 0; --i) {
      let network = this.configuredNetworks[ordered[i]];
      network.priority = newPriority++;

      // Note: networkUpdated declared below since it happens logically after
      // this loop.
      WifiManager.updateNetwork(network, networkUpdated);
    }

    function networkUpdated(ok) {
      if (!ok)
        ++errors;
      if (++done === total) {
        if (errors > 0) {
          callback(false);
          return;
        }

        WifiManager.saveConfig(function(ok) {
          if (!ok) {
            callback(false);
            return;
          }

          self._reloadConfiguredNetworks(function(ok) {
            callback(ok);
          });
        });
      }
    }
  },

  // nsIWifi

  _domManagers: [],
  _fireEvent: function(message, data) {
    this._domManagers.forEach(function(manager) {
      // Note: We should never have a dead message manager here because we
      // observe our child message managers shutting down, below.
      manager.sendAsyncMessage("WifiManager:" + message, data);
    });
  },

  _sendMessage: function(message, success, data, msg) {
    msg.manager.sendAsyncMessage(message + (success ? ":OK" : ":NO"),
                                 { data: data, rid: msg.rid, mid: msg.mid });
  },

  receiveMessage: function MessageManager_receiveMessage(aMessage) {
    let msg = aMessage.data || {};
    msg.manager = aMessage.target;

    // Note: By the time we receive child-process-shutdown, the child process
    // has already forgotten its permissions so we do this before the
    // permissions check.
    if (aMessage.name === "child-process-shutdown") {
      let i;
      if ((i = this._domManagers.indexOf(msg.manager)) != -1) {
        this._domManagers.splice(i, 1);
      }

      return;
    }

    if (!aMessage.target.assertPermission("wifi-manage")) {
      return;
    }

    switch (aMessage.name) {
      case "WifiManager:getNetworks":
        this.getNetworks(msg);
        break;
      case "WifiManager:getKnownNetworks":
        this.getKnownNetworks(msg);
        break;
      case "WifiManager:associate":
        this.associate(msg);
        break;
      case "WifiManager:forget":
        this.forget(msg);
        break;
      case "WifiManager:wps":
        this.wps(msg);
        break;
      case "WifiManager:setPowerSavingMode":
        this.setPowerSavingMode(msg);
        break;
      case "WifiManager:setStaticIpMode":
        this.setStaticIpMode(msg);
        break;
      case "WifiManager:getState": {
        let i;
        if ((i = this._domManagers.indexOf(msg.manager)) === -1) {
          this._domManagers.push(msg.manager);
        }

        let net = this.currentNetwork ? netToDOM(this.currentNetwork) : null;
        return { network: net,
                 connectionInfo: this._lastConnectionInfo,
                 enabled: WifiManager.enabled,
                 status: translateState(WifiManager.state),
                 macAddress: this.macAddress };
      }
    }
  },

  getNetworks: function(msg) {
    const message = "WifiManager:getNetworks:Return";
    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    let sent = false;
    let callback = (function (networks) {
      if (sent)
        return;
      sent = true;
      this._sendMessage(message, networks !== null, networks, msg);
    }).bind(this);
    this.waitForScan(callback);

    WifiManager.scan(true, (function(ok) {
      // If the scan command succeeded, we're done.
      if (ok)
        return;

      // Avoid sending multiple responses.
      if (sent)
        return;

      // Otherwise, let the client know that it failed, it's responsible for
      // trying again in a few seconds.
      sent = true;
      this._sendMessage(message, false, "ScanFailed", msg);
    }).bind(this));
  },

  getWifiScanResults: function(callback) {
    var count = 0;
    var timer = null;
    var self = this;

    self.waitForScan(waitForScanCallback);
    doScan();
    function doScan() {
      WifiManager.scan(true, function (ok) {
        if (!ok) {
          if (!timer) {
            count = 0;
            timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
          }

          if (count++ >= 3) {
            timer = null;
            this.wantScanResults.splice(this.wantScanResults.indexOf(waitForScanCallback), 1);
            callback.onfailure();
            return;
          }

          // Else it's still running, continue waiting.
          timer.initWithCallback(doScan, 10000, Ci.nsITimer.TYPE_ONE_SHOT);
          return;
        }
      });
    }

    function waitForScanCallback(networks) {
      if (networks === null) {
        callback.onfailure();
        return;
      }

      var wifiScanResults = new Array();
      var net;
      for (let net in networks) {
        let value = networks[net];
        wifiScanResults.push(transformResult(value));
      }
      callback.onready(wifiScanResults.length, wifiScanResults);
    }

    function transformResult(element) {
      var result = new WifiScanResult();
      result.connected = false;
      for (let id in element) {
        if (id === "__exposedProps__") {
          continue;
        }
        if (id === "security") {
          result[id] = 0;
          var security = element[id];
          for (let j = 0; j < security.length; j++) {
            if (security[j] === "WPA-PSK") {
              result[id] |= Ci.nsIWifiScanResult.WPA_PSK;
            } else if (security[j] === "WPA-EAP") {
              result[id] |= Ci.nsIWifiScanResult.WPA_EAP;
            } else if (security[j] === "WEP") {
              result[id] |= Ci.nsIWifiScanResult.WEP;
            } else {
             result[id] = 0;
            }
          }
        } else {
          result[id] = element[id];
        }
      }
      return result;
    }
  },

  getKnownNetworks: function(msg) {
    const message = "WifiManager:getKnownNetworks:Return";
    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    this._reloadConfiguredNetworks((function(ok) {
      if (!ok) {
        this._sendMessage(message, false, "Failed", msg);
        return;
      }

      var networks = [];
      for (let networkKey in this.configuredNetworks) {
        networks.push(netToDOM(this.configuredNetworks[networkKey]));
      }

      this._sendMessage(message, true, networks, msg);
    }).bind(this));
  },

  _notifyAfterStateChange: function(success, newState) {
    if (!this._stateRequests.length)
      return;

    // First, notify all of the requests that were trying to make this change.
    let state = this._stateRequests[0].enabled;

    // It is callback function's responsibility to handle the pending request.
    // So we just return here.
    if (this._stateRequests.length > 0
        && ("callback" in this._stateRequests[0])) {
      return;
    }

    // If the new state is not the same as state, then we weren't processing
    // the first request (we were racing somehow) so don't notify.
    if (!success || state === newState) {
      do {
        if (!("callback" in this._stateRequests[0])) {
          this._stateRequests.shift();
        }
        // Don't remove more than one request if the previous one failed.
      } while (success &&
               this._stateRequests.length &&
               !("callback" in this._stateRequests[0]) &&
               this._stateRequests[0].enabled === state);
    }
    // If there were requests queued after this one, run them.
    if (this._stateRequests.length > 0) {
      let self = this;
      this._callbackTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this._callbackTimer.initWithCallback(function(timer) {
        if ("callback" in self._stateRequests[0]) {
          self._stateRequests[0].callback.call(self, self._stateRequests[0].enabled);
        } else {
          WifiManager.setWifiEnabled(self._stateRequests[0].enabled,
                                     self._setWifiEnabledCallback.bind(this));
        }
        timer = null;
      }, 1000, Ci.nsITimer.TYPE_ONE_SHOT);
    }
  },

  _setWifiEnabledCallback: function(status) {
    if (status === "no change") {
      this._notifyAfterStateChange(true, this._stateRequests[0].enabled);
      return;
    }

    if (status) {
      // Don't call notifyAndContinue because we don't want to skip another
      // attempt to turn wifi on or off if this one failed.
      this._notifyAfterStateChange(false, this._stateRequests[0].enabled);
      return;
    }

    // If we're enabling ourselves, then wait until we've connected to the
    // supplicant to notify. If we're disabling, we take care of this in
    // supplicantlost.
    if (WifiManager.supplicantStarted)
      WifiManager.start();
  },

  setWifiEnabled: function(msg) {
    // There are two problems that we're trying to solve here:
    //   - If we get multiple requests to turn on and off wifi before the
    //     current request has finished, then we need to queue up the requests
    //     and handle each on/off request in turn.
    //   - Because we can't pass a callback to WifiManager.start, we need to
    //     have a way to communicate with our onsupplicantconnection callback.
    this._stateRequests.push(msg);
    if (this._stateRequests.length === 1) {
      if ("callback" in this._stateRequests[0]) {
        this._stateRequests[0].callback.call(this, msg.enabled);
      } else {
        WifiManager.setWifiEnabled(msg.enabled, this._setWifiEnabledCallback.bind(this));
      }
    }
  },

  queueRequest: function(enabled, callback) {
    this.setWifiEnabled({enabled: enabled, callback: callback});
  },

  getWifiTetheringParameters: function getWifiTetheringParameters(enable) {
    let ssid;
    let securityType;
    let securityId;
    let interfaceIp;
    let prefix;
    let wifiDhcpStartIp;
    let wifiDhcpEndIp;
    let usbDhcpStartIp;
    let usbDhcpEndIp;
    let dns1;
    let dns2;

    ssid = this.tetheringSettings[SETTINGS_WIFI_SSID];
    securityType = this.tetheringSettings[SETTINGS_WIFI_SECURITY_TYPE];
    securityId = this.tetheringSettings[SETTINGS_WIFI_SECURITY_PASSWORD];
    interfaceIp = this.tetheringSettings[SETTINGS_WIFI_IP];
    prefix = this.tetheringSettings[SETTINGS_WIFI_PREFIX];
    wifiDhcpStartIp = this.tetheringSettings[SETTINGS_WIFI_DHCPSERVER_STARTIP];
    wifiDhcpEndIp = this.tetheringSettings[SETTINGS_WIFI_DHCPSERVER_ENDIP];
    usbDhcpStartIp = this.tetheringSettings[SETTINGS_USB_DHCPSERVER_STARTIP];
    usbDhcpEndIp = this.tetheringSettings[SETTINGS_USB_DHCPSERVER_ENDIP];
    dns1 = this.tetheringSettings[SETTINGS_WIFI_DNS1];
    dns2 = this.tetheringSettings[SETTINGS_WIFI_DNS2];

    // Check the format to prevent netd from crash.
    if (!ssid || ssid == "") {
      debug("Invalid SSID value.");
      return null;
    }
    if (securityType != WIFI_SECURITY_TYPE_NONE &&
        securityType != WIFI_SECURITY_TYPE_WPA_PSK &&
        securityType != WIFI_SECURITY_TYPE_WPA2_PSK) {

      debug("Invalid security type.");
      return null;
    }
    if (securityType != WIFI_SECURITY_TYPE_NONE && !securityId) {
      debug("Invalid security password.");
      return null;
    }
    // Using the default values here until application supports these settings.
    if (interfaceIp == "" || prefix == "" ||
        wifiDhcpStartIp == "" || wifiDhcpEndIp == "" ||
        usbDhcpStartIp == "" || usbDhcpEndIp == "") {
      debug("Invalid subnet information.");
      return null;
    }

    return {
      ssid: ssid,
      security: securityType,
      key: securityId,
      ip: interfaceIp,
      prefix: prefix,
      wifiStartIp: wifiDhcpStartIp,
      wifiEndIp: wifiDhcpEndIp,
      usbStartIp: usbDhcpStartIp,
      usbEndIp: usbDhcpEndIp,
      dns1: dns1,
      dns2: dns2,
      enable: enable,
      mode: enable ? WIFI_FIRMWARE_AP : WIFI_FIRMWARE_STATION,
      link: enable ? NETWORK_INTERFACE_UP : NETWORK_INTERFACE_DOWN
    };
  },

  setWifiApEnabled: function(enabled, callback) {
    let configuration = this.getWifiTetheringParameters(enabled);

    if (!configuration) {
      debug("Invalid Wifi Tethering configuration.");
      return;
    }

    WifiManager.setWifiApEnabled(enabled, configuration, callback);
  },

  associate: function(msg) {
    const MAX_PRIORITY = 9999;
    const message = "WifiManager:associate:Return";
    let network = msg.data;
    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    let privnet = network;
    let self = this;
    function networkReady() {
      // saveConfig now before we disable most of the other networks.
      function selectAndConnect() {
        WifiManager.enableNetwork(privnet.netId, true, function (ok) {
          if (ok)
            self._needToEnableNetworks = true;
          if (WifiManager.state === "DISCONNECTED" ||
              WifiManager.state === "SCANNING") {
            WifiManager.reconnect(function (ok) {
              self._sendMessage(message, ok, ok, msg);
            });
          } else {
            self._sendMessage(message, ok, ok, msg);
          }
        });
      }

      if (self._highestPriority >= MAX_PRIORITY)
        self._reprioritizeNetworks(selectAndConnect);
      else
        WifiManager.saveConfig(selectAndConnect);
    }

    let ssid = privnet.ssid;
    let networkKey = getNetworkKey(privnet);
    let configured;

    if (networkKey in this._addingNetworks) {
      this._sendMessage(message, false, "Racing associates");
      return;
    }

    if (networkKey in this.configuredNetworks)
      configured = this.configuredNetworks[networkKey];

    netFromDOM(privnet, configured);

    privnet.priority = ++this._highestPriority;
    if (configured) {
      privnet.netId = configured.netId;
      WifiManager.updateNetwork(privnet, (function(ok) {
        if (!ok) {
          this._sendMessage(message, false, "Network is misconfigured", msg);
          return;
        }

        networkReady();
      }).bind(this));
    } else {
      // networkReady, above, calls saveConfig. We want to remember the new
      // network as being enabled, which isn't the default, so we explicitly
      // set it to being "enabled" before we add it and save the
      // configuration.
      privnet.disabled = 0;
      this._addingNetworks[networkKey] = privnet;
      WifiManager.addNetwork(privnet, (function(ok) {
        delete this._addingNetworks[networkKey];

        if (!ok) {
          this._sendMessage(message, false, "Network is misconfigured", msg);
          return;
        }

        this.configuredNetworks[networkKey] = privnet;
        networkReady();
      }).bind(this));
    }
  },

  forget: function(msg) {
    const message = "WifiManager:forget:Return";
    let network = msg.data;
    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    this._reloadConfiguredNetworks((function(ok) {
      // Give it a chance to remove the network even if reload is failed.
      if (!ok) {
        debug("Warning !!! Failed to reload the configured networks");
      }

      let ssid = network.ssid;
      let networkKey = getNetworkKey(network);
      if (!(networkKey in this.configuredNetworks)) {
        this._sendMessage(message, false, "Trying to forget an unknown network", msg);
        return;
      }

      let self = this;
      let configured = this.configuredNetworks[networkKey];
      this._reconnectOnDisconnect = (this.currentNetwork &&
                                    (this.currentNetwork.ssid === ssid));
      WifiManager.removeNetwork(configured.netId, function(ok) {
        if (!ok) {
          self._sendMessage(message, false, "Unable to remove the network", msg);
          self._reconnectOnDisconnect = false;
          return;
        }

        WifiManager.saveConfig(function() {
          self._reloadConfiguredNetworks(function() {
            self._sendMessage(message, true, true, msg);
          });
        });
      });
    }).bind(this));
  },

  wps: function(msg) {
    const message = "WifiManager:wps:Return";
    let self = this;
    let detail = msg.data;
    if (detail.method === "pbc") {
      WifiManager.wpsPbc(function(ok) {
        if (ok)
          self._sendMessage(message, true, true, msg);
        else
          self._sendMessage(message, false, "WPS PBC failed", msg);
      });
    } else if (detail.method === "pin") {
      WifiManager.wpsPin(detail, function(pin) {
        if (pin)
          self._sendMessage(message, true, pin, msg);
        else
          self._sendMessage(message, false, "WPS PIN failed", msg);
      });
    } else if (detail.method === "cancel") {
      WifiManager.wpsCancel(function(ok) {
        if (ok)
          self._sendMessage(message, true, true, msg);
        else
          self._sendMessage(message, false, "WPS Cancel failed", msg);
      });
    } else {
      self._sendMessage(message, false, "Invalid WPS method=" + detail.method,
                        msg);
    }
  },

  setPowerSavingMode: function(msg) {
    const message = "WifiManager:setPowerSavingMode:Return";
    let self = this;
    let enabled = msg.data;
    let mode = enabled ? "AUTO" : "ACTIVE";

    // Some wifi drivers may not implement this command. Set power mode
    // even if suspend optimization command failed.
    WifiManager.setSuspendOptimizations(enabled, function(ok) {
      WifiManager.setPowerMode(mode, function(ok) {
        if (ok) {
          self._sendMessage(message, true, true, msg);
        } else {
          self._sendMessage(message, false, "Set power saving mode failed", msg);
        }
      });
    });
  },

  setStaticIpMode: function(msg) {
    const message = "WifiManager:setStaticMode:Return";
    let self = this;
    let network = msg.data.network;
    let info = msg.data.info;

    netFromDOM(network, null);

    // To compatiable with DHCP returned info structure, do translation here
    info.ipaddr_str = info.ipaddr;
    info.proxy_str = info.proxy;
    info.gateway_str = info.gateway;
    info.dns1_str = info.dns1;
    info.dns2_str = info.dns2;

    WifiManager.setStaticIpMode(network, info, function(ok) {
      if (ok) {
        self._sendMessage(message, true, true, msg);
      } else {
        self._sendMessage(message, false, "Set static ip mode failed", msg);
      }
    });
  },

  // This is a bit ugly, but works. In particular, this depends on the fact
  // that RadioManager never actually tries to get the worker from us.
  get worker() { throw "Not implemented"; },

  shutdown: function() {
    debug("shutting down ...");
    this.setWifiEnabled({enabled: false});
  },

  nextRequest: function nextRequest() {
    if (this._stateRequests.length <= 0 ||
        !("callback" in this._stateRequests[0])) {
      return;
    }
    this._stateRequests.shift();
    // Serve the pending requests.
    if (this._stateRequests.length > 0) {
      if ("callback" in this._stateRequests[0]) {
        this._stateRequests[0].callback.call(this,
                                             this._stateRequests[0].enabled);
      } else {
        WifiManager.setWifiEnabled(this._stateRequests[0].enabled,
                                   this._setWifiEnabledCallback.bind(this));
      }
    }
  },

  notifyTetheringOn: function notifyTetheringOn() {
    // It's really sad that we don't have an API to notify the wifi
    // hotspot status. Toggle settings to let gaia know that wifi hotspot
    // is enabled.
    this.tetheringSettings[SETTINGS_WIFI_TETHERING_ENABLED] = true;
    gSettingsService.createLock().set(
      SETTINGS_WIFI_TETHERING_ENABLED, true, null, "fromInternalSetting");
    // Check for the next request.
    this.nextRequest();
  },

  notifyTetheringOff: function notifyTetheringOff() {
    // It's really sad that we don't have an API to notify the wifi
    // hotspot status. Toggle settings to let gaia know that wifi hotspot
    // is disabled.
    this.tetheringSettings[SETTINGS_WIFI_TETHERING_ENABLED] = false;
    gSettingsService.createLock().set(
      SETTINGS_WIFI_TETHERING_ENABLED, false, null, "fromInternalSetting");
    // Check for the next request.
    this.nextRequest();
  },

  handleWifiEnabled: function(enabled) {
    if (WifiManager.enabled === enabled) {
      return;
    }
    // Make sure Wifi hotspot is idle before switching to Wifi mode.
    if (enabled && (this.tetheringSettings[SETTINGS_WIFI_TETHERING_ENABLED] ||
         WifiManager.tetheringState != "UNINITIALIZED")) {
      this.queueRequest(false, function(data) {
        this.disconnectedByWifi = true;
        this.setWifiApEnabled(false, this.notifyTetheringOff.bind(this));
      }.bind(this));
    }
    this.setWifiEnabled({enabled: enabled});

    if (!enabled && this.disconnectedByWifi) {
      this.queueRequest(true, function(data) {
        this.disconnectedByWifi = false;
        this.setWifiApEnabled(true, this.notifyTetheringOn.bind(this));
      }.bind(this));
    }
  },

  handleWifiTetheringEnabled: function(enabled) {
    // Make sure Wifi is idle before switching to Wifi hotspot mode.
    if (enabled && (WifiManager.enabled ||
         WifiManager.state != "UNINITIALIZED")) {
      this.disconnectedByWifiTethering = true;
      this.setWifiEnabled({enabled: false});
    }

    this.queueRequest(enabled, function(data) {
      this.setWifiApEnabled(data, this.nextRequest.bind(this));
    }.bind(this));

    if (!enabled && this.disconnectedByWifiTethering) {
      this.disconnectedByWifiTethering = false;
      this.setWifiEnabled({enabled: true});
    }
  },

  // nsIObserver implementation
  observe: function observe(subject, topic, data) {
    // Note that this function gets called for any and all settings changes,
    // so we need to carefully check if we have the one we're interested in.
    // The string we're interested in will be a JSON string that looks like:
    // {"key":"wifi.enabled","value":"true"}.
    if (topic !== kMozSettingsChangedObserverTopic) {
      return;
    }

    let setting = JSON.parse(data);
    // To avoid WifiWorker setting the wifi again, don't need to deal with
    // the "mozsettings-changed" event fired from internal setting.
    if (setting.message && setting.message === "fromInternalSetting") {
      return;
    }

    this.handle(setting.key, setting.value);
  },

  handle: function handle(aName, aResult) {
    switch(aName) {
      case SETTINGS_WIFI_ENABLED:
        this.handleWifiEnabled(aResult)
        break;
      case SETTINGS_WIFI_DEBUG_ENABLED:
        if (aResult === null)
          aResult = false;
        DEBUG = aResult;
        updateDebug();
        break;
      case SETTINGS_WIFI_TETHERING_ENABLED:
        this._oldWifiTetheringEnabledState = this.tetheringSettings[SETTINGS_WIFI_TETHERING_ENABLED];
        // Fall through!
      case SETTINGS_WIFI_SSID:
      case SETTINGS_WIFI_SECURITY_TYPE:
      case SETTINGS_WIFI_SECURITY_PASSWORD:
      case SETTINGS_WIFI_IP:
      case SETTINGS_WIFI_PREFIX:
      case SETTINGS_WIFI_DHCPSERVER_STARTIP:
      case SETTINGS_WIFI_DHCPSERVER_ENDIP:
      case SETTINGS_WIFI_DNS1:
      case SETTINGS_WIFI_DNS2:
      case SETTINGS_USB_DHCPSERVER_STARTIP:
      case SETTINGS_USB_DHCPSERVER_ENDIP:
        if (aResult !== null) {
          this.tetheringSettings[aName] = aResult;
        }
        debug("'" + aName + "'" + " is now " + this.tetheringSettings[aName]);
        let index = this._wifiTetheringSettingsToRead.indexOf(aName);

        if (index != -1) {
          this._wifiTetheringSettingsToRead.splice(index, 1);
        }

        if (this._wifiTetheringSettingsToRead.length) {
          debug("We haven't read completely the wifi Tethering data from settings db.");
          break;
        }

        if (this._oldWifiTetheringEnabledState === this.tetheringSettings[SETTINGS_WIFI_TETHERING_ENABLED]) {
          debug("No changes for SETTINGS_WIFI_TETHERING_ENABLED flag. Nothing to do.");
          break;
        }

        if (this._oldWifiTetheringEnabledState === null &&
            !this.tetheringSettings[SETTINGS_WIFI_TETHERING_ENABLED]) {
          debug("Do nothing when initial settings for SETTINGS_WIFI_TETHERING_ENABLED flag is false.");
          break;
        }

        this.handleWifiTetheringEnabled(aResult)
        break;
    };
  },

  handleError: function handleError(aErrorMessage) {
    debug("There was an error while reading Tethering settings.");
    this.tetheringSettings = {};
    this.tetheringSettings[SETTINGS_WIFI_TETHERING_ENABLED] = false;
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WifiWorker]);

let debug;
function updateDebug() {
  if (DEBUG) {
    debug = function (s) {
      dump("-*- WifiWorker component: " + s + "\n");
    };
  } else {
    debug = function (s) {};
  }
  WifiManager.syncDebug();
}
updateDebug();
