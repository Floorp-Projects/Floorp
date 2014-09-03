/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/systemlibs.js");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/WifiCommand.jsm");
Cu.import("resource://gre/modules/WifiNetUtil.jsm");
Cu.import("resource://gre/modules/WifiP2pManager.jsm");
Cu.import("resource://gre/modules/WifiP2pWorkerObserver.jsm");

var DEBUG = false; // set to true to show debug messages.

const WIFIWORKER_CONTRACTID = "@mozilla.org/wifi/worker;1";
const WIFIWORKER_CID        = Components.ID("{a14e8977-d259-433a-a88d-58dd44657e5b}");

const WIFIWORKER_WORKER     = "resource://gre/modules/wifi_worker.js";

const kMozSettingsChangedObserverTopic   = "mozsettings-changed";

const MAX_RETRIES_ON_AUTHENTICATION_FAILURE = 2;
const MAX_SUPPLICANT_LOOP_ITERATIONS = 4;
const MAX_RETRIES_ON_DHCP_FAILURE = 2;

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

const DEFAULT_WLAN_INTERFACE = "wlan0";

const DRIVER_READY_WAIT = 2000;

const SUPP_PROP = "init.svc.wpa_supplicant";
const WPA_SUPPLICANT = "wpa_supplicant";
const DHCP_PROP = "init.svc.dhcpcd";
const DHCP = "dhcpcd";

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkManager",
                                   "@mozilla.org/network/manager;1",
                                   "nsINetworkManager");

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkService",
                                   "@mozilla.org/network/service;1",
                                   "nsINetworkService");

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
  var manager = {};

  function getStartupPrefs() {
    return {
      sdkVersion: parseInt(libcutils.property_get("ro.build.version.sdk"), 10),
      unloadDriverEnabled: libcutils.property_get("ro.moz.wifi.unloaddriver") === "1",
      schedScanRecovery: libcutils.property_get("ro.moz.wifi.sched_scan_recover") === "false" ? false : true,
      driverDelay: libcutils.property_get("ro.moz.wifi.driverDelay"),
      p2pSupported: libcutils.property_get("ro.moz.wifi.p2p_supported") === "1",
      eapSimSupported: libcutils.property_get("ro.moz.wifi.eapsim_supported") === "1",
      ifname: libcutils.property_get("wifi.interface")
    };
  }

  let {sdkVersion, unloadDriverEnabled, schedScanRecovery,
       driverDelay, p2pSupported, eapSimSupported, ifname} = getStartupPrefs();

  let capabilities = {
    security: ["OPEN", "WEP", "WPA-PSK", "WPA-EAP"],
    eapMethod: ["PEAP", "TTLS"],
    eapPhase2: ["MSCHAPV2"],
    certificate: ["SERVER"]
  };
  if (eapSimSupported) {
    capabilities.eapMethod.unshift("SIM");
  }

  let wifiListener = {
    onWaitEvent: function(event, iface) {
      if (manager.ifname === iface && handleEvent(event)) {
        waitForEvent(iface);
      } else if (p2pSupported) {
        if (WifiP2pManager.INTERFACE_NAME === iface) {
          // If the connection is closed, wifi.c::wifi_wait_for_event()
          // will still return 'CTRL-EVENT-TERMINATING  - connection closed'
          // rather than blocking. So when we see this special event string,
          // just return immediately.
          const TERMINATED_EVENT = 'CTRL-EVENT-TERMINATING  - connection closed';
          if (-1 !== event.indexOf(TERMINATED_EVENT)) {
            return;
          }
          p2pManager.handleEvent(event);
          waitForEvent(iface);
        }
      }
    },

    onCommand: function(event, iface) {
      onmessageresult(event, iface);
    }
  }

  manager.ifname = ifname;
  manager.connectToSupplicant = false;
  // Emulator build runs to here.
  // The debug() should only be used after WifiManager.
  if (!ifname) {
    manager.ifname = DEFAULT_WLAN_INTERFACE;
  }
  manager.schedScanRecovery = schedScanRecovery;
  manager.driverDelay = driverDelay ? parseInt(driverDelay, 10) : DRIVER_READY_WAIT;

  // Regular Wifi stuff.
  var netUtil = WifiNetUtil(controlMessage);
  var wifiCommand = WifiCommand(controlMessage, manager.ifname, sdkVersion);

  // Wifi P2P stuff
  var p2pManager;
  if (p2pSupported) {
    let p2pCommand = WifiCommand(controlMessage, WifiP2pManager.INTERFACE_NAME, sdkVersion);
    p2pManager = WifiP2pManager(p2pCommand, netUtil);
  }

  let wifiService = Cc["@mozilla.org/wifi/service;1"];
  if (wifiService) {
    wifiService = wifiService.getService(Ci.nsIWifiProxyService);
    let interfaces = [manager.ifname];
    if (p2pSupported) {
      interfaces.push(WifiP2pManager.INTERFACE_NAME);
    }
    wifiService.start(wifiListener, interfaces, interfaces.length);
  } else {
    debug("No wifi service component available!");
  }

  // Callbacks to invoke when a reply arrives from the wifi service.
  var controlCallbacks = Object.create(null);
  var idgen = 0;

  function controlMessage(obj, callback) {
    var id = idgen++;
    obj.id = id;
    if (callback) {
      controlCallbacks[id] = callback;
    }
    wifiService.sendCommand(obj, obj.iface);
  }

  let onmessageresult = function(data, iface) {
    var id = data.id;
    var callback = controlCallbacks[id];
    if (callback) {
      callback(data);
      delete controlCallbacks[id];
    }
  }

  // Polling the status worker
  var recvErrors = 0;

  function waitForEvent(iface) {
    wifiService.waitForEvent(iface);
  }

  // Commands to the control worker.

  var driverLoaded = false;

  function loadDriver(callback) {
    if (driverLoaded) {
      callback(0);
      return;
    }

    wifiCommand.loadDriver(function (status) {
      driverLoaded = (status >= 0);
      callback(status)
    });
  }

  function unloadDriver(type, callback) {
    if (!unloadDriverEnabled) {
      // Unloading drivers is generally unnecessary and
      // can trigger bugs in some drivers.
      // On properly written drivers, bringing the interface
      // down powers down the interface.
      if (type === WIFI_FIRMWARE_STATION) {
        notify("supplicantlost", { success: true });
      }
      callback(0);
      return;
    }

    wifiCommand.unloadDriver(function(status) {
      driverLoaded = (status < 0);
      if (type === WIFI_FIRMWARE_STATION) {
        notify("supplicantlost", { success: true });
      }
      callback(status);
    });
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
    wifiCommand.setBackgroundScan(manager.backgroundScanEnabled, callback);
  }

  var scanModeActive = false;

  function scan(forceActive, callback) {
    if (forceActive && !scanModeActive) {
      // Note: we ignore errors from doSetScanMode.
      wifiCommand.doSetScanMode(true, function(ignore) {
        setBackgroundScan("OFF", function(turned, ignore) {
          reEnableBackgroundScan = turned;
          manager.handlePreWifiScan();
          wifiCommand.scan(function(ok) {
            wifiCommand.doSetScanMode(false, function(ignore) {
              // The result of scanCommand is the result of the actual SCAN
              // request.
              callback(ok);
            });
          });
        });
      });
      return;
    }
    manager.handlePreWifiScan();
    wifiCommand.scan(callback);
  }

  var debugEnabled = false;

  function syncDebug() {
    if (debugEnabled !== DEBUG) {
      let wanted = DEBUG;
      wifiCommand.setLogLevel(wanted ? "DEBUG" : "INFO", function(ok) {
        if (ok)
          debugEnabled = wanted;
      });
      if (p2pSupported && p2pManager) {
        p2pManager.setDebug(DEBUG);
      }
    }
  }

  function getDebugEnabled(callback) {
    wifiCommand.getLogLevel(function(level) {
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

  function setScanMode(setActive, callback) {
    scanModeActive = setActive;
    wifiCommand.doSetScanMode(setActive, callback);
  }

  var httpProxyConfig = Object.create(null);

  /**
   * Given a network, configure http proxy when using wifi.
   * @param network A network object to update http proxy
   * @param info Info should have following field:
   *        - httpProxyHost ip address of http proxy.
   *        - httpProxyPort port of http proxy, set 0 to use default port 8080.
   * @param callback callback function.
   */
  function configureHttpProxy(network, info, callback) {
    if (!network)
      return;

    let networkKey = getNetworkKey(network);

    if (!info || info.httpProxyHost === "") {
      delete httpProxyConfig[networkKey];
    } else {
      httpProxyConfig[networkKey] = network;
      httpProxyConfig[networkKey].httpProxyHost = info.httpProxyHost;
      httpProxyConfig[networkKey].httpProxyPort = info.httpProxyPort;
    }

    callback(true);
  }

  function getHttpProxyNetwork(network) {
    if (!network)
      return null;

    let networkKey = getNetworkKey(network);
    return ((networkKey in httpProxyConfig) ? httpProxyConfig : null);
  }

  function setHttpProxy(network) {
    if (!network)
      return;

    gNetworkService.setNetworkProxy(network);
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
      info.ipaddr = netHelpers.stringToIP(info.ipaddr_str);
      info.gateway = netHelpers.stringToIP(info.gateway_str);
      info.mask_str = netHelpers.ipToString(netHelpers.makeMask(info.maskLength));

      // Optional
      info.dns1 = netHelpers.stringToIP(info.dns1_str);
      info.dns2 = netHelpers.stringToIP(info.dns2_str);
      info.proxy = netHelpers.stringToIP(info.proxy_str);

      staticIpConfig[setNetworkKey] = info;

      // If the ssid of current connection is the same as configured ssid
      // It means we need update current connection to use static IP address.
      if (setNetworkKey == curNetworkKey) {
        // Use configureInterface directly doesn't work, the network iterface
        // and routing table is changed but still cannot connect to network
        // so the workaround here is disable interface the enable again to
        // trigger network reconnect with static ip.
        netUtil.disableInterface(manager.ifname, function (ok) {
          netUtil.enableInterface(manager.ifname, function (ok) {
          });
        });
      }
    });
  }

  var dhcpInfo = null;

  function runStaticIp(ifname, key) {
    debug("Run static ip");

    // Read static ip information from settings.
    let staticIpInfo;

    if (!(key in staticIpConfig))
      return;

    staticIpInfo = staticIpConfig[key];

    // Stop dhcpd when use static IP
    if (dhcpInfo != null) {
      netUtil.stopDhcp(manager.ifname, function() {});
    }

    // Set ip, mask length, gateway, dns to network interface
    netUtil.configureInterface( { ifname: ifname,
                                  ipaddr: staticIpInfo.ipaddr,
                                  mask: staticIpInfo.maskLength,
                                  gateway: staticIpInfo.gateway,
                                  dns1: staticIpInfo.dns1,
                                  dns2: staticIpInfo.dns2 }, function (data) {
      netUtil.runIpConfig(ifname, staticIpInfo, function(data) {
        dhcpInfo = data.info;
        notify("networkconnected", data);
      });
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
    // Detect wpa_supplicant's loop iterations.
    manager.supplicantLoopDetection(fields.prevState, fields.state);
    notify("statechange", fields);

    // Don't update state when and after disabling.
    if (manager.state === "DISABLING" ||
        manager.state === "UNINITIALIZED") {
      return false;
    }

    manager.state = fields.state;
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
      connectTries = 0;
      recvErrors = 0;
      manager.connectToSupplicant = true;
      didConnectSupplicant(function(){});
      return;
    }
    if (connectTries++ < 5) {
      // Try again in 1 seconds.
      if (!retryTimer)
        retryTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

      retryTimer.initWithCallback(function(timer) {
        wifiCommand.connectToSupplicant(connectCallback);
      }, 1000, Ci.nsITimer.TYPE_ONE_SHOT);
      return;
    }

    retryTimer = null;
    connectTries = 0;
    notify("supplicantlost", { success: false });
  }

  manager.connectionDropped = function(callback) {
    // Reset network interface when connection drop
    netUtil.configureInterface( { ifname: manager.ifname,
                                  ipaddr: 0,
                                  mask: 0,
                                  gateway: 0,
                                  dns1: 0,
                                  dns2: 0 }, function (data) {
    });

    // If we got disconnected, kill the DHCP client in preparation for
    // reconnection.
    netUtil.resetConnections(manager.ifname, function() {
      netUtil.stopDhcp(manager.ifname, function() {
        callback();
      });
    });
  }

  manager.start = function() {
    debug("detected SDK version " + sdkVersion);
    wifiCommand.connectToSupplicant(connectCallback);
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
      netUtil.runDhcp(manager.ifname, function(data) {
        dhcpInfo = data.info;
        if (!dhcpInfo) {
          if (++manager.dhcpFailuresCount >= MAX_RETRIES_ON_DHCP_FAILURE) {
            manager.dhcpFailuresCount = 0;
            notify("disconnected", {connectionInfo: manager.connectionInfo});
            return;
          }
          // NB: We have to call disconnect first. Otherwise, we only reauth with
          // the existing AP and don't retrigger DHCP.
          manager.disconnect(function() {
            manager.reassociate(function(){});
          });
          return;
        }

        manager.dhcpFailuresCount = 0;
        notify("networkconnected", data);
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

  manager.getNetworkId = function (ssid, callback) {
    manager.getConfiguredNetworks(function(networks) {
      if (!networks) {
        debug("Unable to get configured networks");
        return callback(null);
      }
      for (let net in networks) {
        let network = networks[net];
        // Trying to get netId from
        // 1. network matching SSID if SSID is provided.
        // 2. current network if no SSID is provided, it's not guaranteed that
        //    current network matches requested SSID.
        if ((!ssid && network.status === "CURRENT") ||
            (ssid && network.ssid && ssid === dequote(network.ssid))) {
          return callback(net);
        }
      }
      callback(null);
    });
  }

  function handleWpaEapEvents(event) {
    if (event.indexOf("CTRL-EVENT-EAP-FAILURE") !== -1) {
      if (event.indexOf("EAP authentication failed") !== -1) {
        notify("passwordmaybeincorrect");
        if (manager.authenticationFailuresCount > MAX_RETRIES_ON_AUTHENTICATION_FAILURE) {
          manager.authenticationFailuresCount = 0;
          notify("disconnected", {connectionInfo: manager.connectionInfo});
        }
      }
      return true;
    }
    if (event.indexOf("CTRL-EVENT-EAP-TLS-CERT-ERROR") !== -1) {
      // Cert Error
      notify("passwordmaybeincorrect");
      if (manager.authenticationFailuresCount > MAX_RETRIES_ON_AUTHENTICATION_FAILURE) {
        manager.authenticationFailuresCount = 0;
        notify("disconnected", {connectionInfo: manager.connectionInfo});
      }
      return true;
    }
    if (event.indexOf("CTRL-EVENT-EAP-STARTED") !== -1) {
      notifyStateChange({ state: "AUTHENTICATING" });
      return true;
    }
    return true;
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
          notify("disconnected", {connectionInfo: manager.connectionInfo});
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
      // As long the monitor socket is not closed and we haven't seen too many
      // recv errors yet, we will keep going for a bit longer.
      if (event.indexOf("connection closed") === -1 &&
          event.indexOf("recv error") !== -1 && ++recvErrors < 10)
        return true;

      notifyStateChange({ state: "DISCONNECTED", BSSID: null, id: -1 });

      // If the supplicant is terminated as commanded, the supplicant lost
      // notification will be sent after driver unloaded. In such case, the
      // manager state will be "DISABLING" or "UNINITIALIZED".
      // So if supplicant terminated with incorrect manager state, implying
      // unexpected condition, we should notify supplicant lost here.
      if (manager.state !== "DISABLING" && manager.state !== "UNINITIALIZED") {
        notify("supplicantlost", { success: true });
      }

      if (manager.stopSupplicantCallback) {
        cancelWaitForTerminateEventTimer();
        // It's possible that the terminating event triggered by timer comes
        // earlier than the event from wpa_supplicant. Since
        // stopSupplicantCallback contains async. callbacks, swap it to local
        // to prevent calling the callback twice.
        let stopSupplicantCallback = manager.stopSupplicantCallback.bind(manager);
        manager.stopSupplicantCallback = null;
        stopSupplicantCallback();
        stopSupplicantCallback = null;
      }
      return false;
    }
    if (eventData.indexOf("CTRL-EVENT-DISCONNECTED") === 0) {
      var token = event.split(" ")[1];
      var bssid = token.split("=")[1];
      if (manager.authenticationFailuresCount > MAX_RETRIES_ON_AUTHENTICATION_FAILURE) {
        manager.authenticationFailuresCount = 0;
        notify("disconnected", {connectionInfo: manager.connectionInfo});
      }
      manager.connectionInfo.bssid = null;
      manager.connectionInfo.ssid = null;
      manager.connectionInfo.id = -1;
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
      manager.handlePostWifiScan();
      notify("scanresultsavailable");
      return true;
    }
    if (eventData.indexOf("CTRL-EVENT-EAP") === 0) {
      return handleWpaEapEvents(event);
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

  function didConnectSupplicant(callback) {
    waitForEvent(manager.ifname);

    // Load up the supplicant state.
    getDebugEnabled(function(ok) {
      syncDebug();
    });
    wifiCommand.status(function(status) {
      parseStatus(status);
      notify("supplicantconnection");
      callback();
    });

    if (p2pSupported) {
      manager.enableP2p(function(success) {});
    }
  }

  function prepareForStartup(callback) {
    let status = libcutils.property_get(DHCP_PROP + "_" + manager.ifname);
    if (status !== "running") {
      tryStopSupplicant();
      return;
    }
    manager.connectionDropped(function() {
      tryStopSupplicant();
    });

    // Ignore any errors and kill any currently-running supplicants. On some
    // phones, stopSupplicant won't work for a supplicant that we didn't
    // start, so we hand-roll it here.
    function tryStopSupplicant () {
      let status = libcutils.property_get(SUPP_PROP);
      if (status !== "running") {
        callback();
        return;
      }
      suppressEvents = true;
      wifiCommand.killSupplicant(function() {
        netUtil.disableInterface(manager.ifname, function (ok) {
          suppressEvents = false;
          callback();
        });
      });
    }
  }

  // Initial state.
  manager.state = "UNINITIALIZED";
  manager.tetheringState = "UNINITIALIZED";
  manager.supplicantStarted = false;
  manager.connectionInfo = { ssid: null, bssid: null, id: -1 };
  manager.authenticationFailuresCount = 0;
  manager.loopDetectionCount = 0;
  manager.dhcpFailuresCount = 0;
  manager.stopSupplicantCallback = null;

  manager.__defineGetter__("enabled", function() {
    switch (manager.state) {
      case "UNINITIALIZED":
      case "INITIALIZING":
      case "DISABLING":
        return false;
      default:
        return true;
    }
  });

  var waitForTerminateEventTimer = null;
  function cancelWaitForTerminateEventTimer() {
    if (waitForTerminateEventTimer) {
      waitForTerminateEventTimer.cancel();
      waitForTerminateEventTimer = null;
    }
  };
  function createWaitForTerminateEventTimer(onTimeout) {
    if (waitForTerminateEventTimer) {
      return;
    }
    waitForTerminateEventTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    waitForTerminateEventTimer.initWithCallback(onTimeout,
                                                4000,
                                                Ci.nsITimer.TYPE_ONE_SHOT);
  };

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
                                             manager.driverDelay,
                                             Ci.nsITimer.TYPE_ONE_SHOT);
  };

  // Public interface of the wifi service.
  manager.setWifiEnabled = function(enabled, callback) {
    if (enabled === manager.isWifiEnabled(manager.state)) {
      callback("no change");
      return;
    }

    if (enabled) {
      manager.state = "INITIALIZING";
      // Register as network interface.
      WifiNetworkInterface.name = manager.ifname;
      if (!WifiNetworkInterface.registered) {
        gNetworkManager.registerNetworkInterface(WifiNetworkInterface);
        WifiNetworkInterface.registered = true;
      }
      WifiNetworkInterface.state = Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED;
      WifiNetworkInterface.ips = [];
      WifiNetworkInterface.prefixLengths = [];
      WifiNetworkInterface.gateways = [];
      WifiNetworkInterface.dnses = [];
      gNetworkManager.updateNetworkInterface(WifiNetworkInterface);

      prepareForStartup(function() {
        loadDriver(function (status) {
          if (status < 0) {
            callback(status);
            manager.state = "UNINITIALIZED";
            return;
          }
          // This command is mandatory for Nexus 4. But some devices like
          // Galaxy S2 don't support it. Continue to start wpa_supplicant
          // even if we fail to set wifi operation mode to station.
          gNetworkService.setWifiOperationMode(manager.ifname,
                                               WIFI_FIRMWARE_STATION,
                                               function (status) {

            function startSupplicantInternal() {
              wifiCommand.startSupplicant(function (status) {
                if (status < 0) {
                  unloadDriver(WIFI_FIRMWARE_STATION, function() {
                    callback(status);
                  });
                  manager.state = "UNINITIALIZED";
                  return;
                }

                manager.supplicantStarted = true;
                netUtil.enableInterface(manager.ifname, function (ok) {
                  callback(ok ? 0 : -1);
                });
              });
            }

            function doStartSupplicant() {
              cancelWaitForDriverReadyTimer();

              if (!manager.connectToSupplicant) {
                startSupplicantInternal();
                return;
              }
              wifiCommand.closeSupplicantConnection(function () {
                manager.connectToSupplicant = false;
                // closeSupplicantConnection() will trigger onsupplicantlost
                // and set manager.state to "UNINITIALIZED", we have to
                // restore it here.
                manager.state = "INITIALIZING";
                startSupplicantInternal();
              });
            }
            // Driver startup on certain platforms takes longer than it takes for us
            // to return from loadDriver, so wait 2 seconds before starting
            // the supplicant to give it a chance to start.
            if (manager.driverDelay > 0) {
              createWaitForDriverReadyTimer(doStartSupplicant);
            } else {
              doStartSupplicant();
            }
          });
        });
      });
    } else {
      manager.state = "DISABLING";
      // Note these following calls ignore errors. If we fail to kill the
      // supplicant gracefully, then we need to continue telling it to die
      // until it does.
      let doDisableWifi = function() {
        manager.stopSupplicantCallback = (function () {
          wifiCommand.stopSupplicant(function (status) {
            wifiCommand.closeSupplicantConnection(function() {
              manager.connectToSupplicant = false;
              manager.state = "UNINITIALIZED";
              netUtil.disableInterface(manager.ifname, function (ok) {
                unloadDriver(WIFI_FIRMWARE_STATION, callback);
              });
            });
          });
        }).bind(this);

        let terminateEventCallback = (function() {
          handleEvent("CTRL-EVENT-TERMINATING");
        }).bind(this);
        createWaitForTerminateEventTimer(terminateEventCallback);

        // We are going to terminate the connection between wpa_supplicant.
        // Stop the polling timer immediately to prevent connection info update
        // command blocking in control thread until socket timeout.
        notify("stopconnectioninfotimer");

        wifiCommand.terminateSupplicant(function (ok) {
          manager.connectionDropped(function () {
          });
        });
      }

      if (p2pSupported) {
        p2pManager.setEnabled(false, { onDisabled: doDisableWifi });
      } else {
        doDisableWifi();
      }
    }
  }

  var wifiHotspotStatusTimer = null;
  function cancelWifiHotspotStatusTimer() {
    if (wifiHotspotStatusTimer) {
      wifiHotspotStatusTimer.cancel();
      wifiHotspotStatusTimer = null;
    }
  }

  function createWifiHotspotStatusTimer(onTimeout) {
    wifiHotspotStatusTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    wifiHotspotStatusTimer.init(onTimeout, 5000, Ci.nsITimer.TYPE_REPEATING_SLACK);
  }

  // Get wifi interface and load wifi driver when enable Ap mode.
  manager.setWifiApEnabled = function(enabled, configuration, callback) {
    if (enabled === manager.isWifiTetheringEnabled(manager.tetheringState)) {
      callback("no change");
      return;
    }

    if (enabled) {
      manager.tetheringState = "INITIALIZING";
      loadDriver(function (status) {
        if (status < 0) {
          callback();
          manager.tetheringState = "UNINITIALIZED";
          if (wifiHotspotStatusTimer) {
            cancelWifiHotspotStatusTimer();
            wifiCommand.closeHostapdConnection(function(result) {
            });
          }
          return;
        }

        function getWifiHotspotStatus() {
          wifiCommand.hostapdGetStations(function(result) {
            notify("stationinfoupdate", {station: result});
          });
        }

        function doStartWifiTethering() {
          cancelWaitForDriverReadyTimer();
          WifiNetworkInterface.name = libcutils.property_get("wifi.tethering.interface", manager.ifname);
          gNetworkManager.setWifiTethering(enabled, WifiNetworkInterface,
                                           configuration, function(result) {
            if (result) {
              manager.tetheringState = "UNINITIALIZED";
            } else {
              manager.tetheringState = "COMPLETED";
              wifiCommand.connectToHostapd(function(result) {
                if (result) {
                  return;
                }
                // Create a timer to track the connection status.
                createWifiHotspotStatusTimer(getWifiHotspotStatus);
              });
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
    } else {
      cancelWifiHotspotStatusTimer();
      gNetworkManager.setWifiTethering(enabled, WifiNetworkInterface,
                                       configuration, function(result) {
        // Should we fire a dom event if we fail to set wifi tethering  ?
        debug("Disable Wifi tethering result: " + (result ? result : "successfully"));
        // Unload wifi driver even if we fail to control wifi tethering.
        unloadDriver(WIFI_FIRMWARE_AP, function(status) {
          if (status < 0) {
            debug("Fail to unload wifi driver");
          }
          manager.tetheringState = "UNINITIALIZED";
          callback();
        });
      });
    }
  }

  manager.disconnect = wifiCommand.disconnect;
  manager.reconnect = wifiCommand.reconnect;
  manager.reassociate = wifiCommand.reassociate;

  var networkConfigurationFields = [
    {name: "ssid",          type: "string"},
    {name: "bssid",         type: "string"},
    {name: "psk",           type: "string"},
    {name: "wep_key0",      type: "string"},
    {name: "wep_key1",      type: "string"},
    {name: "wep_key2",      type: "string"},
    {name: "wep_key3",      type: "string"},
    {name: "wep_tx_keyidx", type: "integer"},
    {name: "priority",      type: "integer"},
    {name: "key_mgmt",      type: "string"},
    {name: "scan_ssid",     type: "string"},
    {name: "disabled",      type: "integer"},
    {name: "identity",      type: "string"},
    {name: "password",      type: "string"},
    {name: "auth_alg",      type: "string"},
    {name: "phase1",        type: "string"},
    {name: "phase2",        type: "string"},
    {name: "eap",           type: "string"},
    {name: "pin",           type: "string"},
    {name: "pcsc",          type: "string"},
    {name: "ca_cert",       type: "string"},
    {name: "subject_match", type: "string"}
  ];

  manager.getNetworkConfiguration = function(config, callback) {
    var netId = config.netId;
    var done = 0;
    for (var n = 0; n < networkConfigurationFields.length; ++n) {
      let fieldName = networkConfigurationFields[n].name;
      let fieldType = networkConfigurationFields[n].type;
      wifiCommand.getNetworkVariable(netId, fieldName, function(value) {
        if (value !== null) {
          if (fieldType === "integer") {
            config[fieldName] = parseInt(value, 10);
          } else {
            // value is string type by default.
            config[fieldName] = value;
          }
        }
        if (++done == networkConfigurationFields.length)
          callback(config);
      });
    }
  }
  manager.setNetworkConfiguration = function(config, callback) {
    var netId = config.netId;
    var done = 0;
    var errors = 0;

    function hasValidProperty(name) {
      return ((name in config) &&
               config[name] != null &&
               (["password", "wep_key0", "psk"].indexOf(name) === -1 ||
                config[name] !== '*'));
    }

    for (var n = 0; n < networkConfigurationFields.length; ++n) {
      let fieldName = networkConfigurationFields[n].name;
      if (!hasValidProperty(fieldName)) {
        ++done;
      } else {
        wifiCommand.setNetworkVariable(netId, fieldName, config[fieldName], function(ok) {
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
    wifiCommand.listNetworks(function (reply) {
      var networks = Object.create(null);
      var lines = reply ? reply.split("\n") : 0;
      if (lines.length <= 1) {
        // We need to make sure we call the callback even if there are no
        // configured networks.
        callback(networks);
        return;
      }

      var done = 0;
      var errors = 0;
      for (var n = 1; n < lines.length; ++n) {
        var result = lines[n].split("\t");
        var netId = parseInt(result[0], 10);
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
                wifiCommand.removeNetwork(netId, function() {
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
    wifiCommand.addNetwork(function (netId) {
      config.netId = netId;
      manager.setNetworkConfiguration(config, function (ok) {
        if (!ok) {
          wifiCommand.removeNetwork(netId, function() { callback(false); });
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
    wifiCommand.removeNetwork(netId, callback);
  }

  manager.saveConfig = function(callback) {
    wifiCommand.saveConfig(callback);
  }
  manager.enableNetwork = function(netId, disableOthers, callback) {
    if (p2pSupported) {
      // We have to stop wifi direct scan before associating to an AP.
      // Otherwise we will get a "REJECT" wpa supplicant event.
      p2pManager.setScanEnabled(false, function(success) {});
    }
    wifiCommand.enableNetwork(netId, disableOthers, callback);
  }
  manager.disableNetwork = function(netId, callback) {
    wifiCommand.disableNetwork(netId, callback);
  }
  manager.getMacAddress = wifiCommand.getMacAddress;
  manager.getScanResults = wifiCommand.scanResults;
  manager.setScanMode = function(mode, callback) {
    setScanMode(mode === "active", callback); // Use our own version.
  }
  manager.setBackgroundScan = setBackgroundScan; // Use our own version.
  manager.scan = scan; // Use our own version.
  manager.wpsPbc = wifiCommand.wpsPbc;
  manager.wpsPin = wifiCommand.wpsPin;
  manager.wpsCancel = wifiCommand.wpsCancel;
  manager.setPowerMode = (sdkVersion >= 16)
                         ? wifiCommand.setPowerModeJB
                         : wifiCommand.setPowerModeICS;
  manager.getHttpProxyNetwork = getHttpProxyNetwork;
  manager.setHttpProxy = setHttpProxy;
  manager.configureHttpProxy = configureHttpProxy;
  manager.setSuspendOptimizations = (sdkVersion >= 16)
                                   ? wifiCommand.setSuspendOptimizationsJB
                                   : wifiCommand.setSuspendOptimizationsICS;
  manager.setStaticIpMode = setStaticIpMode;
  manager.getRssiApprox = wifiCommand.getRssiApprox;
  manager.getLinkSpeed = wifiCommand.getLinkSpeed;
  manager.getDhcpInfo = function() { return dhcpInfo; }
  manager.getConnectionInfo = (sdkVersion >= 15)
                              ? wifiCommand.getConnectionInfoICS
                              : wifiCommand.getConnectionInfoGB;

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

  manager.isWifiEnabled = function(state) {
    switch (state) {
      case "UNINITIALIZED":
      case "DISABLING":
        return false;
      default:
        return true;
    }
  }

  manager.isWifiTetheringEnabled = function(state) {
    switch (state) {
      case "UNINITIALIZED":
        return false;
      default:
        return true;
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
          notify("disconnected", {connectionInfo: manager.connectionInfo});
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

  manager.handlePreWifiScan = function() {
    if (p2pSupported) {
      // Before doing regular wifi scan, we have to disable wifi direct
      // scan first. Otherwise we will never get the scan result.
      p2pManager.blockScan();
    }
  };

  manager.handlePostWifiScan = function() {
    if (p2pSupported) {
      // After regular wifi scanning, we should restore the restricted
      // wifi direct scan.
      p2pManager.unblockScan();
    }
  };

  //
  // Public APIs for P2P.
  //

  manager.p2pSupported = function() {
    return p2pSupported;
  };

  manager.getP2pManager = function() {
    return p2pManager;
  };

  manager.enableP2p = function(callback) {
    p2pManager.setEnabled(true, {
      onSupplicantConnected: function() {
        waitForEvent(WifiP2pManager.INTERFACE_NAME);
      },

      onEnabled: function(success) {
        callback(success);
      }
    });
  };

  manager.getCapabilities = function() {
    return capabilities;
  }

  // Cert Services
  let wifiCertService = Cc["@mozilla.org/wifi/certservice;1"];
  if (wifiCertService) {
    wifiCertService = wifiCertService.getService(Ci.nsIWifiCertService);
    wifiCertService.start(wifiListener);
  } else {
    debug("No wifi CA service component available");
  }

  manager.importCert = function(caInfo, callback) {
    var id = idgen++;
    if (callback) {
      controlCallbacks[id] = callback;
    }

    wifiCertService.importCert(id, caInfo.certBlob, caInfo.certPassword,
                               caInfo.certNickname);
  }

  manager.deleteCert = function(caInfo, callback) {
    var id = idgen++;
    if (callback) {
      controlCallbacks[id] = callback;
    }

    wifiCertService.deleteCert(id, caInfo.certNickname);
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
    } else if (key_mgmt.indexOf("WPA-EAP") != -1) {
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
  pin: "rw",
  phase1: "rw",
  phase2: "rw",
  serverCertificate: "rw"
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

  ips: [],

  prefixLengths: [],

  dnses: [],

  gateways: [],

  httpProxyHost: null,

  httpProxyPort: null,

  getAddresses: function (ips, prefixLengths) {
    ips.value = this.ips.slice();
    prefixLengths.value = this.prefixLengths.slice();

    return this.ips.length;
  },

  getGateways: function (count) {
    if (count) {
      count.value = this.gateways.length;
    }
    return this.gateways.slice();
  },

  getDnses: function (count) {
    if (count) {
      count.value = this.dnses.length;
    }
    return this.dnses.slice();
  }
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
                    "WifiManager:setHttpProxy",
                    "WifiManager:setStaticIpMode",
                    "WifiManager:importCert",
                    "WifiManager:getImportedCerts",
                    "WifiManager:deleteCert",
                    "WifiManager:setWifiEnabled",
                    "WifiManager:setWifiTethering",
                    "child-process-shutdown"];

  messages.forEach((function(msgName) {
    this._mm.addMessageListener(msgName, this);
  }).bind(this));

  Services.obs.addObserver(this, kMozSettingsChangedObserverTopic, false);
  Services.obs.addObserver(this, "xpcom-shutdown", false);

  this.wantScanResults = [];

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
  this.macAddress = null;

  this._lastConnectionInfo = null;
  this._connectionInfoTimer = null;
  this._reconnectOnDisconnect = false;

  // Create p2pObserver and assign to p2pManager.
  if (WifiManager.p2pSupported()) {
    this._p2pObserver = WifiP2pWorkerObserver(WifiManager.getP2pManager());
    WifiManager.getP2pManager().setObserver(this._p2pObserver);

    // Add DOM message observerd by p2pObserver to the message listener as well.
    this._p2pObserver.getObservedDOMMessages().forEach((function(msgName) {
      this._mm.addMessageListener(msgName, this);
    }).bind(this));
  }

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
    WifiManager.handlePostWifiScan();
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
                   (net.key_mgmt && net.key_mgmt !== "NONE") ? [net.key_mgmt.split(" ")[0]] :
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
    if ("ca_cert" in net && net.ca_cert &&
        net.ca_cert.indexOf("keystore://WIFI_SERVERCERT_" === 0)) {
      pub.serverCertificate = net.ca_cert.substr(27);
    }
    if(net.subject_match) {
      pub.subjectMatch = net.subject_match;
    }
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
      } else if (net.keyManagement === "WPA-EAP") {
        net.keyManagement += " IEEE8021X";
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

    function hasValidProperty(name) {
      return ((name in net) && net[name] != null);
    }

    if (hasValidProperty("eap")) {
      if (hasValidProperty("pin")) {
        net.pin = quote(net.pin);
      }

      if (hasValidProperty("phase1"))
        net.phase1 = quote(net.phase1);

      if (hasValidProperty("phase2")) {
        if (net.phase2 === "TLS") {
          net.phase2 = quote("autheap=" + net.phase2);
        } else { // PAP, MSCHAPV2, etc.
          net.phase2 = quote("auth=" + net.phase2);
        }
      }

      if (hasValidProperty("serverCertificate"))
        net.ca_cert = quote("keystore://WIFI_SERVERCERT_" + net.serverCertificate);

      if (hasValidProperty("subjectMatch"))
        net.subject_match = quote(net.subjectMatch);
    }

    return net;
  };

  WifiManager.onsupplicantconnection = function() {
    debug("Connected to supplicant");
    // Give it a state other than UNINITIALIZED, INITIALIZING or DISABLING
    // defined in getter function of WifiManager.enabled. It implies that
    // we are ready to accept dom request from applications.
    WifiManager.state = "DISCONNECTED";
    self._reloadConfiguredNetworks(function(ok) {
      // Prime this.networks.
      if (!ok)
        return;

      // The select network command we used in associate() disables others networks.
      // Enable them here to make sure wpa_supplicant helps to connect to known
      // network automatically.
      self._enableAllNetworks();
      WifiManager.saveConfig(function() {})
    });

    // Notify everybody, even if they didn't ask us to come up.
    WifiManager.getMacAddress(function (mac) {
      self.macAddress = mac;
      debug("Got mac: " + mac);
      self._fireEvent("wifiUp", { macAddress: mac });
      self.requestDone();
    });

    if (WifiManager.state === "SCANNING")
      startScanStuckTimer();
  };

  WifiManager.onsupplicantlost = function() {
    WifiManager.supplicantStarted = false;
    WifiManager.state = "UNINITIALIZED";
    debug("Supplicant died!");

    // Notify everybody, even if they didn't ask us to come up.
    self._fireEvent("wifiDown", {});
    self.requestDone();
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

    let connectionInfo = this.connectionInfo;
    WifiManager.getNetworkId(connectionInfo.ssid, function(netId) {
      // Trying to get netId from current network.
      if (!netId &&
          self.currentNetwork && self.currentNetwork.ssid &&
          dequote(self.currentNetwork.ssid) == connectionInfo.ssid &&
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

        // Update http proxy when connected to network.
        let netConnect = WifiManager.getHttpProxyNetwork(self.currentNetwork);
        if (netConnect)
          WifiManager.setHttpProxy(netConnect);

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
        // wpa_supplicant may give us a "DISCONNECTED" event even if
        // we are already in "DISCONNECTED" state.
        if ((WifiNetworkInterface.state ===
             Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED) &&
             (this.prevState === "INITIALIZING" ||
              this.prevState === "DISCONNECTED" ||
              this.prevState === "INTERFACE_DISABLED" ||
              this.prevState === "INACTIVE" ||
              this.prevState === "UNINITIALIZED")) {
          return;
        }

        self._fireEvent("ondisconnect", {});

        // When disconnected, clear the http proxy setting if it exists.
        // Temporarily set http proxy to empty and restore user setting after setHttpProxy.
        let netDisconnect = WifiManager.getHttpProxyNetwork(self.currentNetwork);
        if (netDisconnect) {
          let prehttpProxyHostSetting = netDisconnect.httpProxyHost;
          let prehttpProxyPortSetting = netDisconnect.httpProxyPort;

          netDisconnect.httpProxyHost = "";
          netDisconnect.httpProxyPort = 0;

          WifiManager.setHttpProxy(netDisconnect);

          netDisconnect.httpProxyHost = prehttpProxyHostSetting;
          netDisconnect.httpProxyPort = prehttpProxyPortSetting;
        }

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
        WifiNetworkInterface.ips = [];
        WifiNetworkInterface.prefixLengths = [];
        WifiNetworkInterface.gateways = [];
        WifiNetworkInterface.dnses = [];
        gNetworkManager.updateNetworkInterface(WifiNetworkInterface);

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
      case "AUTHENTICATING":
        self._fireEvent("onauthenticating", {network: netToDOM(self.currentNetwork)});
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
    if (!this.info || !this.info.ipaddr_str) {
      debug("Network information is invalid.");
      return;
    }

    let maskLength =
      netHelpers.getMaskLength(netHelpers.stringToIP(this.info.mask_str));
    if (!maskLength) {
      maskLength = 32; // max prefix for IPv4.
    }
    WifiNetworkInterface.state =
      Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED;
    WifiNetworkInterface.ips = [this.info.ipaddr_str];
    WifiNetworkInterface.prefixLengths = [maskLength];
    WifiNetworkInterface.gateways = [this.info.gateway_str];
    if (typeof this.info.dns1_str == "string" &&
        this.info.dns1_str.length) {
      WifiNetworkInterface.dnses.push(this.info.dns1_str);
    }
    if (typeof this.info.dns2_str == "string" &&
        this.info.dns2_str.length) {
      WifiNetworkInterface.dnses.push(this.info.dns2_str);
    }
    gNetworkManager.updateNetworkInterface(WifiNetworkInterface);

    self.ipAddress = this.info.ipaddr_str;

    // We start the connection information timer when we associate, but
    // don't have our IP address until here. Make sure that we fire a new
    // connectionInformation event with the IP address the next time the
    // timer fires.
    self._lastConnectionInfo = null;
    self._fireEvent("onconnect", { network: netToDOM(self.currentNetwork) });
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
        var match = /([\S]+)\s+([\S]+)\s+([\S]+)\s+(\[[\S]+\])?\s(.*)/.exec(lines[i]);

        if (match && match[5]) {
          let ssid = match[5],
              bssid = match[1],
              signalLevel = match[3],
              flags = match[4];

          // Skip ad-hoc networks which aren't supported (bug 811635).
          if (flags && flags.indexOf("[IBSS]") >= 0)
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

  WifiManager.onstationinfoupdate = function() {
    self._fireEvent("stationinfoupdate", { station: this.station });
  };

  WifiManager.onstopconnectioninfotimer = function() {
    self._stopConnectionInfoTimer();
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
                                         Ci.nsIObserver,
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
            last.ipAddress === info.ipAddress &&
            tensPlace(last.relSignalStrength) === tensPlace(info.relSignalStrength)) {
          return;
        }

        self._lastConnectionInfo = info;
        debug("Firing connectioninfoupdate: " + uneval(info));
        self._fireEvent("connectioninfoupdate", info);
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
        delete networks[net];

        if (!network.ssid) {
          WifiManager.removeNetwork(network.netId, function() {});
          continue;
        }

        if (network.hasOwnProperty("priority") &&
            network.priority > this._highestPriority) {
          this._highestPriority = network.priority;
        }

        let networkKey = getNetworkKey(network);
        // Accept latest config of same network(same SSID and same security).
        if (networks[networkKey]) {
          WifiManager.removeNetwork(networks[networkKey].netId, function() {});
        }
        networks[networkKey] = network;
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
    try {
      msg.manager.sendAsyncMessage(message + (success ? ":OK" : ":NO"),
                                   { data: data, rid: msg.rid, mid: msg.mid });
    } catch (e) {
      debug("sendAsyncMessage error : " + e);
    }
    this._splicePendingRequest(msg);
  },

  _domRequest: [],

  _splicePendingRequest: function(msg) {
    for (let i = 0; i < this._domRequest.length; i++) {
      if (this._domRequest[i].msg === msg) {
        this._domRequest.splice(i, 1);
        return;
      }
    }
  },

  _clearPendingRequest: function() {
    if (this._domRequest.length === 0) return;
    this._domRequest.forEach((function(req) {
      this._sendMessage(req.name + ":Return", false, "Wifi is disabled", req.msg);
    }).bind(this));
  },

  receiveMessage: function MessageManager_receiveMessage(aMessage) {
    let msg = aMessage.data || {};
    msg.manager = aMessage.target;

    if (WifiManager.p2pSupported()) {
      // If p2pObserver returns something truthy, return it!
      // Otherwise, continue to do the rest of tasks.
      var p2pRet = this._p2pObserver.onDOMMessage(aMessage);
      if (p2pRet) {
        return p2pRet;
      }
    }

    // Note: By the time we receive child-process-shutdown, the child process
    // has already forgotten its permissions so we do this before the
    // permissions check.
    if (aMessage.name === "child-process-shutdown") {
      let i;
      if ((i = this._domManagers.indexOf(msg.manager)) != -1) {
        this._domManagers.splice(i, 1);
      }
      for (i = this._domRequest.length - 1; i >= 0; i--) {
        if (this._domRequest[i].msg.manager === msg.manager) {
          this._domRequest.splice(i, 1);
        }
      }
      return;
    }

    if (!aMessage.target.assertPermission("wifi-manage")) {
      return;
    }

    // We are interested in DOMRequests only.
    if (aMessage.name != "WifiManager:getState") {
      this._domRequest.push({name: aMessage.name, msg:msg});
    }

    switch (aMessage.name) {
      case "WifiManager:setWifiEnabled":
        this.setWifiEnabled(msg);
        break;
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
      case "WifiManager:setHttpProxy":
        this.setHttpProxy(msg);
        break;
      case "WifiManager:setStaticIpMode":
        this.setStaticIpMode(msg);
        break;
      case "WifiManager:importCert":
        this.importCert(msg);
        break;
      case "WifiManager:getImportedCerts":
        this.getImportedCerts(msg);
        break;
      case "WifiManager:deleteCert":
        this.deleteCert(msg);
        break;
      case "WifiManager:setWifiTethering":
        this.setWifiTethering(msg);
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
                 macAddress: this.macAddress,
                 capabilities: WifiManager.getCapabilities()};
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

    if (!WifiManager.enabled) {
      callback.onfailure();
      return;
    }

    self.waitForScan(waitForScanCallback);
    doScan();
    function doScan() {
      WifiManager.scan(true, (function (ok) {
        if (!ok) {
          if (!timer) {
            count = 0;
            timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
          }

          if (count++ >= 3) {
            timer = null;
            self.wantScanResults.splice(self.wantScanResults.indexOf(waitForScanCallback), 1);
            callback.onfailure();
            return;
          }

          // Else it's still running, continue waiting.
          timer.initWithCallback(doScan, 10000, Ci.nsITimer.TYPE_ONE_SHOT);
          return;
        }
      }).bind(this));
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

  _setWifiEnabledCallback: function(status) {
    if (status !== 0) {
      this.requestDone();
      return;
    }

    // If we're enabling ourselves, then wait until we've connected to the
    // supplicant to notify. If we're disabling, we take care of this in
    // supplicantlost.
    if (WifiManager.supplicantStarted)
      WifiManager.start();
  },

  /**
   * Compatibility flags for detecting if Gaia is controlling wifi by settings
   * or API, once API is called, gecko will no longer accept wifi enable
   * control from settings.
   * This is used to deal with compatibility issue while Gaia adopted to use
   * API but gecko doesn't remove the settings code in time.
   * TODO: Remove this flag in Bug 1050147
   */
  ignoreWifiEnabledFromSettings: false,
  setWifiEnabled: function(msg) {
    const message = "WifiManager:setWifiEnabled:Return";
    let self = this;
    let enabled = msg.data;

    self.ignoreWifiEnabledFromSettings = true;
    // No change.
    if (enabled === WifiManager.enabled) {
      this._sendMessage(message, true, true, msg);
      return;
    }

    // Can't enable wifi while hotspot mode is enabled.
    if (enabled && (this.tetheringSettings[SETTINGS_WIFI_TETHERING_ENABLED] ||
        WifiManager.isWifiTetheringEnabled(WifiManager.tetheringState))) {
      self._sendMessage(message, false, "Can't enable Wifi while hotspot mode is enabled", msg);
      return;
    }

    WifiManager.setWifiEnabled(enabled, function(ok) {
      if (ok === 0 || ok === "no change") {
        self._sendMessage(message, true, true, msg);

        // Reply error to pending requests.
        if (!enabled) {
          self._clearPendingRequest();
        } else {
          WifiManager.start();
        }
      } else {
        self._sendMessage(message, false, "Set wifi enabled failed", msg);
      }
    });
  },

  _setWifiEnabled: function(enabled, callback) {
    // Reply error to pending requests.
    if (!enabled) {
      this._clearPendingRequest();
    }

    WifiManager.setWifiEnabled(enabled, callback);
  },

  // requestDone() must be called to before callback complete(or error)
  // so next queue in the request quene can be executed.
  // TODO: Remove command queue in Bug 1050147
  queueRequest: function(data, callback) {
    if (!callback) {
        throw "Try to enqueue a request without callback";
    }

    let optimizeCommandList = ["setWifiEnabled", "setWifiApEnabled"];
    if (optimizeCommandList.indexOf(data.command) != -1) {
      this._stateRequests = this._stateRequests.filter(function(element) {
        return element.data.command !== data.command;
      });
    }

    this._stateRequests.push({
      data: data,
      callback: callback
    });

    this.nextRequest();
  },

  getWifiTetheringParameters: function getWifiTetheringParameters(enable) {
    if (this.useTetheringAPI) {
      return this.getWifiTetheringConfiguration(enable);
    } else {
      return this.getWifiTetheringParametersBySetting(enable);
    }
  },

  getWifiTetheringConfiguration: function getWifiTetheringConfiguration(enable) {
    let config = {};
    let params = this.tetheringConfig;

    let check = function(field, _default) {
      config[field] = field in params ? params[field] : _default;
    };

    check("ssid", DEFAULT_WIFI_SSID);
    check("security", DEFAULT_WIFI_SECURITY_TYPE);
    check("key", DEFAULT_WIFI_SECURITY_PASSWORD);
    check("ip", DEFAULT_WIFI_IP);
    check("prefix", DEFAULT_WIFI_PREFIX);
    check("wifiStartIp", DEFAULT_WIFI_DHCPSERVER_STARTIP);
    check("wifiEndIp", DEFAULT_WIFI_DHCPSERVER_ENDIP);
    check("usbStartIp", DEFAULT_USB_DHCPSERVER_STARTIP);
    check("usbEndIp", DEFAULT_USB_DHCPSERVER_ENDIP);
    check("dns1", DEFAULT_DNS1);
    check("dns2", DEFAULT_DNS2);

    config.enable = enable;
    config.mode = enable ? WIFI_FIRMWARE_AP : WIFI_FIRMWARE_STATION;
    config.link = enable ? NETWORK_INTERFACE_UP : NETWORK_INTERFACE_DOWN;

    // Check the format to prevent netd from crash.
    if (enable && (!config.ssid || config.ssid == "")) {
      debug("Invalid SSID value.");
      return null;
    }

    if (enable && (config.security != WIFI_SECURITY_TYPE_NONE && !config.key)) {
      debug("Invalid security password.");
      return null;
    }

    // Using the default values here until application supports these settings.
    if (config.ip == "" || config.prefix == "" ||
        config.wifiStartIp == "" || config.wifiEndIp == "" ||
        config.usbStartIp == "" || config.usbEndIp == "") {
      debug("Invalid subnet information.");
      return null;
    }

    return config;
  },

  getWifiTetheringParametersBySetting: function getWifiTetheringParametersBySetting(enable) {
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
      this.requestDone();
      debug("Invalid Wifi Tethering configuration.");
      return;
    }

    WifiManager.setWifiApEnabled(enabled, configuration, callback);
  },

  associate: function(msg) {
    const MAX_PRIORITY = 9999;
    const message = "WifiManager:associate:Return";
    let network = msg.data;

    let privnet = network;
    let dontConnect = privnet.dontConnect;
    delete privnet.dontConnect;

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

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

      var selectAndConnectOrReturn = dontConnect ?
        function() {
          self._sendMessage(message, true, "Wifi has been recorded", msg);
        } : selectAndConnect;
      if (self._highestPriority >= MAX_PRIORITY) {
        self._reprioritizeNetworks(selectAndConnectOrReturn);
      } else {
        WifiManager.saveConfig(selectAndConnectOrReturn);
      }
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
      // Sync priority back to configured so if priority reaches MAX_PRIORITY,
      // it can be sorted correctly in _reprioritizeNetworks() because the
      // function sort network based on priority in configure list.
      configured.priority = privnet.priority;
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
        if (self._needToEnableNetworks) {
          self._enableAllNetworks();
          self._needToEnableNetworks = false;
        }

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

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

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

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

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

  setHttpProxy: function(msg) {
    const message = "WifiManager:setHttpProxy:Return";
    let self = this;
    let network = msg.data.network;
    let info = msg.data.info;

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    WifiManager.configureHttpProxy(network, info, function(ok) {
      if (ok) {
        // If configured network is current connected network
        // need update http proxy immediately.
        let setNetworkKey = getNetworkKey(network);
        let curNetworkKey = self.currentNetwork ? getNetworkKey(self.currentNetwork) : null;
        if (setNetworkKey === curNetworkKey)
          WifiManager.setHttpProxy(network);

        self._sendMessage(message, true, true, msg);
      } else {
        self._sendMessage(message, false, "Set http proxy failed", msg);
      }
    });
  },

  setStaticIpMode: function(msg) {
    const message = "WifiManager:setStaticMode:Return";
    let self = this;
    let network = msg.data.network;
    let info = msg.data.info;

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

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

  importCert: function importCert(msg) {
    const message = "WifiManager:importCert:Return";
    let self = this;

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    WifiManager.importCert(msg.data, function(data) {
      if (data.status === 0) {
        let usageString = ["ServerCert"];
        let usageArray = [];
        for (let i = 0; i < usageString.length; i++) {
          if (data.usageFlag & (0x01 << i)) {
            usageArray.push(usageString[i]);
          }
        }

        self._sendMessage(message, true, {
          nickname: data.nickname,
          usage: usageArray
        }, msg);
      } else {
        self._sendMessage(message, false, "Import Cert failed", msg);
      }
    });
  },

  getImportedCerts: function getImportedCerts(msg) {
    const message = "WifiManager:getImportedCerts:Return";
    let self = this;

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    let certDB = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
    if (!certDB) {
      self._sendMessage(message, false, "Failed to query NSS DB service", msg);
    }

    let certList = certDB.getCerts();
    if (!certList) {
      self._sendMessage(message, false, "Failed to get certificate List", msg);
    }

    let certListEnum = certList.getEnumerator();
    if (!certListEnum) {
      self._sendMessage(message, false, "Failed to get certificate List enumerator", msg);
    }
    let importedCerts = {
      ServerCert: [],
    };
    let UsageMapping = {
      SERVERCERT: "ServerCert",
    };

    while (certListEnum.hasMoreElements()) {
      let certInfo = certListEnum.getNext().QueryInterface(Ci.nsIX509Cert);
      let certNicknameInfo = /WIFI\_([A-Z]*)\_(.*)/.exec(certInfo.nickname);
      if (!certNicknameInfo) {
        continue;
      }
      importedCerts[UsageMapping[certNicknameInfo[1]]].push(certNicknameInfo[2]);
    }

    self._sendMessage(message, true, importedCerts, msg);
  },

  deleteCert: function deleteCert(msg) {
    const message = "WifiManager:deleteCert:Return";
    let self = this;

    if (!WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is disabled", msg);
      return;
    }

    WifiManager.deleteCert(msg.data, function(data) {
      self._sendMessage(message, data.status === 0, "Delete Cert failed", msg);
    });
  },

  // TODO : These two variables should be removed once GAIA uses tethering API.
  useTetheringAPI : false,
  tetheringConfig : {},

  setWifiTethering: function setWifiTethering(msg) {
    const message = "WifiManager:setWifiTethering:Return";
    let self = this;
    let enabled = msg.data.enabled;

    this.useTetheringAPI = true;
    this.tetheringConfig = msg.data.config;

    if (WifiManager.enabled) {
      this._sendMessage(message, false, "Wifi is enabled", msg);
      return;
    }

    this.setWifiApEnabled(enabled, function() {
      if ((enabled && WifiManager.tetheringState == "COMPLETED") ||
          (!enabled && WifiManager.tetheringState == "UNINITIALIZED")) {
        self._sendMessage(message, true, msg.data, msg);
      } else {
        msg.data.reason = enabled ?
          "Enable WIFI tethering faild" : "Disable WIFI tethering faild";
        self._sendMessage(message, false, msg.data, msg);
      }
    });
  },

  // This is a bit ugly, but works. In particular, this depends on the fact
  // that RadioManager never actually tries to get the worker from us.
  get worker() { throw "Not implemented"; },

  shutdown: function() {
    debug("shutting down ...");
    this.queueRequest({command: "setWifiEnabled", value: false}, function(data) {
      this._setWifiEnabled(false, this._setWifiEnabledCallback.bind(this));
    }.bind(this));
  },

  // TODO: Remove command queue in Bug 1050147.
  requestProcessing: false,   // Hold while dequeue and execution a request.
                              // Released upon the request is fully executed,
                              // i.e, mostly after callback is done.
  requestDone: function requestDone() {
    this.requestProcessing = false;
    this.nextRequest();
  },

  nextRequest: function nextRequest() {
    // No request to process
    if (this._stateRequests.length === 0) {
      return;
    }

    // Handling request, wait for it.
    if (this.requestProcessing) {
      return;
    }

    // Hold processing lock
    this.requestProcessing = true;

    // Find next valid request
    let request = this._stateRequests.shift();

    request.callback(request.data);
  },

  notifyTetheringOn: function notifyTetheringOn() {
    // It's really sad that we don't have an API to notify the wifi
    // hotspot status. Toggle settings to let gaia know that wifi hotspot
    // is enabled.
    let self = this;
    this.tetheringSettings[SETTINGS_WIFI_TETHERING_ENABLED] = true;
    this._oldWifiTetheringEnabledState = true;
    gSettingsService.createLock().set(
      SETTINGS_WIFI_TETHERING_ENABLED,
      true,
      {
        handle: function(aName, aResult) {
          self.requestDone();
        },
        handleError: function(aErrorMessage) {
          self.requestDone();
        }
      });
  },

  notifyTetheringOff: function notifyTetheringOff() {
    // It's really sad that we don't have an API to notify the wifi
    // hotspot status. Toggle settings to let gaia know that wifi hotspot
    // is disabled.
    let self = this;
    this.tetheringSettings[SETTINGS_WIFI_TETHERING_ENABLED] = false;
    this._oldWifiTetheringEnabledState = false;
    gSettingsService.createLock().set(
      SETTINGS_WIFI_TETHERING_ENABLED,
      false,
      {
        handle: function(aName, aResult) {
          self.requestDone();
        },
        handleError: function(aErrorMessage) {
          self.requestDone();
        }
      });
  },

  handleWifiEnabled: function(enabled) {
    if (this.ignoreWifiEnabledFromSettings) {
      return;
    }

    // Make sure Wifi hotspot is idle before switching to Wifi mode.
    if (enabled) {
      this.queueRequest({command: "setWifiApEnabled", value: false}, function(data) {
        if (this.tetheringSettings[SETTINGS_WIFI_TETHERING_ENABLED] ||
            WifiManager.isWifiTetheringEnabled(WifiManager.tetheringState)) {
          this.disconnectedByWifi = true;
          this.setWifiApEnabled(false, this.notifyTetheringOff.bind(this));
        } else {
          this.requestDone();
        }
      }.bind(this));
    }

    this.queueRequest({command: "setWifiEnabled", value: enabled}, function(data) {
      this._setWifiEnabled(enabled, this._setWifiEnabledCallback.bind(this));
    }.bind(this));

    if (!enabled) {
      this.queueRequest({command: "setWifiApEnabled", value: true}, function(data) {
        if (this.disconnectedByWifi) {
          this.setWifiApEnabled(true, this.notifyTetheringOn.bind(this));
        } else {
          this.requestDone();
        }
        this.disconnectedByWifi = false;
      }.bind(this));
    }
  },

  handleWifiTetheringEnabled: function(enabled) {
    // Make sure Wifi is idle before switching to Wifi hotspot mode.
    if (enabled) {
      this.queueRequest({command: "setWifiEnabled", value: false}, function(data) {
        if (WifiManager.isWifiEnabled(WifiManager.state)) {
          this.disconnectedByWifiTethering = true;
          this._setWifiEnabled(false, this._setWifiEnabledCallback.bind(this));
        } else {
          this.requestDone();
        }
      }.bind(this));
    }

    this.queueRequest({command: "setWifiApEnabled", value: enabled}, function(data) {
      this.setWifiApEnabled(enabled, this.requestDone.bind(this));
    }.bind(this));

    if (!enabled) {
      this.queueRequest({command: "setWifiEnabled", value: true}, function(data) {
        if (this.disconnectedByWifiTethering) {
          this._setWifiEnabled(true, this._setWifiEnabledCallback.bind(this));
        } else {
          this.requestDone();
        }
        this.disconnectedByWifiTethering = false;
      }.bind(this));
    }
  },

  // nsIObserver implementation
  observe: function observe(subject, topic, data) {
    switch (topic) {
    case kMozSettingsChangedObserverTopic:
      // The string we're interested in will be a JSON string that looks like:
      // {"key":"wifi.enabled","value":"true"}.

      let setting = JSON.parse(data);
      // To avoid WifiWorker setting the wifi again, don't need to deal with
      // the "mozsettings-changed" event fired from internal setting.
      if (setting.isInternalChange) {
        return;
      }

      this.handle(setting.key, setting.value);
      break;

    case "xpcom-shutdown":
      let wifiService = Cc["@mozilla.org/wifi/service;1"].getService(Ci.nsIWifiProxyService);
      wifiService.shutdown();
      let wifiCertService = Cc["@mozilla.org/wifi/certservice;1"].getService(Ci.nsIWifiCertService);
      wifiCertService.shutdown();
      break;
    }
  },

  handle: function handle(aName, aResult) {
    switch(aName) {
      // TODO: Remove function call in Bug 1050147.
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
        // TODO: code related to wifi-tethering setting should be removed after GAIA
        //       use tethering API
        if (this.useTetheringAPI) {
          break;
        }

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

        this._oldWifiTetheringEnabledState = this.tetheringSettings[SETTINGS_WIFI_TETHERING_ENABLED];
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
