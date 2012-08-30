/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const NETWORKMANAGER_CONTRACTID = "@mozilla.org/network/manager;1";
const NETWORKMANAGER_CID =
  Components.ID("{33901e46-33b8-11e1-9869-f46d04d25bcc}");
const NETWORKINTERFACE_CONTRACTID = "@mozilla.org/network/interface;1";
const NETWORKINTERFACE_CID =
  Components.ID("{266c3edd-78f0-4512-8178-2d6fee2d35ee}");

const DEFAULT_PREFERRED_NETWORK_TYPE = Ci.nsINetworkInterface.NETWORK_TYPE_WIFI;

XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

XPCOMUtils.defineLazyGetter(this, "gWifi", function () {
  return Cc["@mozilla.org/telephony/system-worker-manager;1"]
           .getService(Ci.nsIInterfaceRequestor)
           .getInterface(Ci.nsIWifi);
});

const TOPIC_INTERFACE_STATE_CHANGED  = "network-interface-state-changed";
const TOPIC_INTERFACE_REGISTERED     = "network-interface-registered";
const TOPIC_INTERFACE_UNREGISTERED   = "network-interface-unregistered";
const TOPIC_DEFAULT_ROUTE_CHANGED    = "network-default-route-changed";
const TOPIC_MOZSETTINGS_CHANGED      = "mozsettings-changed";
const TOPIC_XPCOM_SHUTDOWN           = "xpcom-shutdown";

// TODO, get USB RNDIS interface name automatically.(see Bug 776212)
const DEFAULT_USB_INTERFACE_NAME  = "rndis0";
const DEFAULT_3G_INTERFACE_NAME   = "rmnet0";
const DEFAULT_WIFI_INTERFACE_NAME = "wlan0";

const TETHERING_TYPE_WIFI = "WiFi";
const TETHERING_TYPE_USB  = "USB";

const USB_FUNCTION_RNDIS = "rndis,adb";
const USB_FUNCTION_ADB   = "adb";

// 2xx - Requested action has been successfully completed.
const NETD_COMMAND_OKAY  = 200;
const NETD_COMMAND_ERROR = 300;

const WIFI_FIRMWARE_AP            = "AP";
const WIFI_FIRMWARE_STATION       = "STA";
const WIFI_SECURITY_TYPE_NONE     = "open";
const WIFI_SECURITY_TYPE_WPA_PSK  = "wpa-psk";
const WIFI_SECURITY_TYPE_WPA2_PSK = "wpa2-psk";
const WIFI_CTRL_INTERFACE         = "wl0.1";

const NETWORK_INTERFACE_UP   = "up";
const NETWORK_INTERFACE_DOWN = "down";

// Settings DB path for Wifi tethering.
const SETTINGS_WIFI_ENABLED            = "tethering.wifi.enabled";
const SETTINGS_WIFI_SSID               = "tethering.wifi.ssid";
const SETTINGS_WIFI_SECURITY_TYPE      = "tethering.wifi.security.type";
const SETTINGS_WIFI_SECURITY_PASSWORD  = "tethering.wifi.security.password";
const SETTINGS_WIFI_IP                 = "tethering.wifi.ip";
const SETTINGS_WIFI_PREFIX             = "tethering.wifi.prefix";
const SETTINGS_WIFI_DHCPSERVER_STARTIP = "tethering.wifi.dhcpserver.startip";
const SETTINGS_WIFI_DHCPSERVER_ENDIP   = "tethering.wifi.dhcpserver.endip";

// Settings DB path for USB tethering.
const SETTINGS_USB_ENABLED             = "tethering.usb.enabled";
const SETTINGS_USB_IP                  = "tethering.usb.ip";
const SETTINGS_USB_PREFIX              = "tethering.usb.prefix";
const SETTINGS_USB_DHCPSERVER_STARTIP  = "tethering.usb.dhcpserver.startip";
const SETTINGS_USB_DHCPSERVER_ENDIP    = "tethering.usb.dhcpserver.endip";

const MANUAL_PROXY_CONFIGURATION = 1;

const DEBUG = false;

/**
 * This component watches for network interfaces changing state and then
 * adjusts routes etc. accordingly.
 */
function NetworkManager() {
  this.networkInterfaces = {};
  Services.obs.addObserver(this, TOPIC_INTERFACE_STATE_CHANGED, true);
  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN, false);
  Services.obs.addObserver(this, TOPIC_MOZSETTINGS_CHANGED, false);

  debug("Starting worker.");
  this.worker = new ChromeWorker("resource://gre/modules/net_worker.js");
  this.worker.onmessage = function onmessage(event) {
    this.handleWorkerMessage(event);
  }.bind(this);
  this.worker.onerror = function onerror(event) {
    debug("Received error from worker: " + event.filename +
          ":" + event.lineno + ": " + event.message + "\n");
    // Prevent the event from bubbling any further.
    event.preventDefault();
  };

  // Callbacks to invoke when a reply arrives from the net_worker.
  this.controlCallbacks = Object.create(null);

  // Default values for internal and external interfaces.
  this._tetheringInterface = Object.create(null);
  this._tetheringInterface[TETHERING_TYPE_USB] = {externalInterface: DEFAULT_3G_INTERFACE_NAME,
                                                  internalInterface: DEFAULT_USB_INTERFACE_NAME};
  this._tetheringInterface[TETHERING_TYPE_WIFI] = {externalInterface: DEFAULT_3G_INTERFACE_NAME,
                                                   internalInterface: DEFAULT_WIFI_INTERFACE_NAME};

  this.tetheringSettings[SETTINGS_WIFI_ENABLED] = false;
  this.tetheringSettings[SETTINGS_USB_ENABLED] = false;

  let settingsLock = gSettingsService.getLock();
  // Read wifi tethering data from settings DB.
  settingsLock.get(SETTINGS_WIFI_SSID, this);
  settingsLock.get(SETTINGS_WIFI_SECURITY_TYPE, this);
  settingsLock.get(SETTINGS_WIFI_SECURITY_PASSWORD, this);
  settingsLock.get(SETTINGS_WIFI_IP, this);
  settingsLock.get(SETTINGS_WIFI_PREFIX, this);
  settingsLock.get(SETTINGS_WIFI_DHCPSERVER_STARTIP, this);
  settingsLock.get(SETTINGS_WIFI_DHCPSERVER_ENDIP, this);
  // Read usb tethering data from settings DB.
  settingsLock.get(SETTINGS_USB_IP, this);
  settingsLock.get(SETTINGS_USB_PREFIX, this);
  settingsLock.get(SETTINGS_USB_DHCPSERVER_STARTIP, this);
  settingsLock.get(SETTINGS_USB_DHCPSERVER_ENDIP, this);
}
NetworkManager.prototype = {
  classID:   NETWORKMANAGER_CID,
  classInfo: XPCOMUtils.generateCI({classID: NETWORKMANAGER_CID,
                                    contractID: NETWORKMANAGER_CONTRACTID,
                                    classDescription: "Network Manager",
                                    interfaces: [Ci.nsINetworkManager]}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkManager,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver,
                                         Ci.nsIWorkerHolder,
                                         Ci.nsISettingsServiceCallback]),

  // nsIWorkerHolder

  worker: null,

  // nsIObserver

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case TOPIC_INTERFACE_STATE_CHANGED:
        let network = subject.QueryInterface(Ci.nsINetworkInterface);
        debug("Network " + network.name + " changed state to " + network.state);
        switch (network.state) {
          case Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED:
          case Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED:
            this.setAndConfigureActive();
            break;
        }
        break;
      case TOPIC_MOZSETTINGS_CHANGED:
        let setting = JSON.parse(data);
        this.handle(setting.key, setting.value);
        break;
      case TOPIC_XPCOM_SHUTDOWN:
        Services.obs.removeObserver(this, TOPIC_XPCOM_SHUTDOWN);
        Services.obs.removeObserver(this, TOPIC_MOZSETTINGS_CHANGED);
        Services.obs.removeObserver(this, TOPIC_INTERFACE_STATE_CHANGED);
        break;
    }
  },

  // nsINetworkManager

  registerNetworkInterface: function registerNetworkInterface(network) {
    if (!(network instanceof Ci.nsINetworkInterface)) {
      throw Components.Exception("Argument must be nsINetworkInterface.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    if (network.name in this.networkInterfaces) {
      throw Components.Exception("Network with that name already registered!",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    this.networkInterfaces[network.name] = network;
    this.setAndConfigureActive();
    Services.obs.notifyObservers(network, TOPIC_INTERFACE_REGISTERED, null);
    debug("Network '" + network.name + "' registered.");
  },

  unregisterNetworkInterface: function unregisterNetworkInterface(network) {
    if (!(network instanceof Ci.nsINetworkInterface)) {
      throw Components.Exception("Argument must be nsINetworkInterface.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    if (!(network.name in this.networkInterfaces)) {
      throw Components.Exception("No network with that name registered.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    delete this.networkInterfaces[network.name];
    this.setAndConfigureActive();
    Services.obs.notifyObservers(network, TOPIC_INTERFACE_UNREGISTERED, null);
    debug("Network '" + network.name + "' unregistered.");
  },

  networkInterfaces: null,

  _preferredNetworkType: DEFAULT_PREFERRED_NETWORK_TYPE,
  get preferredNetworkType() {
    return this._preferredNetworkType;
  },
  set preferredNetworkType(val) {
    if ([Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
         Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,
         Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS].indexOf(val) == -1) {
      throw "Invalid network type";
    }
    this._preferredNetworkType = val;
  },

  active: null,
  _overriddenActive: null,

  overrideActive: function overrideActive(network) {
    this._overriddenActive = network;
    this.setAndConfigureActive();
  },

  // Helpers

  controlMessage: function controlMessage(params, callback) {
    if (callback) {
      let id = callback.name;
      params.id = id;
      this.controlCallbacks[id] = callback;
    }
    this.worker.postMessage(params);
  },

  handleWorkerMessage: function handleWorkerMessage(e) {
    let response = e.data;
    let id = response.id;
    let callback = this.controlCallbacks[id];
    if (callback) {
      callback.call(this, response);
      delete this.controlCallbacks[id];
    }
    debug("NetworkManager received message from worker: " + JSON.stringify(e));
  },

  /**
   * Determine the active interface and configure it.
   */
  setAndConfigureActive: function setAndConfigureActive() {
    debug("Evaluating whether active network needs to be changed.");
    let oldActive = this.active;

    if (this._overriddenActive) {
      debug("We have an override for the active network: " +
            this._overriddenActive.name);
      // The override was just set, so reconfigure the network.
      if (this.active != this._overriddenActive) {
        this.active = this._overriddenActive;
        this.setDefaultRouteAndDNS(oldActive);
      }
      return;
    }

    // If the active network is already of the preferred type, nothing to do.
    if (this.active &&
        this.active.state == Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED &&
        this.active.type == this._preferredNetworkType) {
      debug("Active network is already our preferred type. Not doing anything.");
      return;
    }

    // Find a suitable network interface to activate.
    this.active = null;
    for each (let network in this.networkInterfaces) {
      if (network.state != Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED) {
        continue;
      }
      this.active = network;
      if (network.type == this.preferredNetworkType) {
        debug("Found our preferred type of network: " + network.name);
        break;
      }
    }
    if (this.active) {
      this.setDefaultRouteAndDNS(oldActive);
    }
    Services.io.offline = !this.active;
  },

  setDefaultRouteAndDNS: function setDefaultRouteAndDNS(oldInterface) {
    debug("Going to change route and DNS to " + this.active.name);
    let options = {
      cmd: this.active.dhcp ? "runDHCPAndSetDefaultRouteAndDNS" : "setDefaultRouteAndDNS",
      ifname: this.active.name,
      oldIfname: (oldInterface && oldInterface != this.active) ? oldInterface.name : null
    };
    this.worker.postMessage(options);
    this.setNetworkProxy();
  },

  setNetworkProxy: function setNetworkProxy() {
    try {
      if (!this.active.httpProxyHost || this.active.httpProxyHost == "") {
        // Sets direct connection to internet.
        Services.prefs.clearUserPref("network.proxy.type");
        Services.prefs.clearUserPref("network.proxy.share_proxy_settings");
        Services.prefs.clearUserPref("network.proxy.http");
        Services.prefs.clearUserPref("network.proxy.http_port");
        debug("No proxy support for " + this.active.name + " network interface.");
        return;
      }

      debug("Going to set proxy settings for " + this.active.name + " network interface.");
      // Sets manual proxy configuration.
      Services.prefs.setIntPref("network.proxy.type", MANUAL_PROXY_CONFIGURATION);
      // Do not use this proxy server for all protocols.
      Services.prefs.setBoolPref("network.proxy.share_proxy_settings", false);
      Services.prefs.setCharPref("network.proxy.http", this.active.httpProxyHost);
      let port = this.active.httpProxyPort == "" ? 8080 : this.active.httpProxyPort;
      Services.prefs.setIntPref("network.proxy.http_port", port);
    } catch (ex) {
       debug("Exception " + ex + ". Unable to set proxy setting for "
             + this.active.name + " network interface.");
       return;
    }
  },

  // nsISettingsServiceCallback

  tetheringSettings: {},

  handle: function handle(aName, aResult) {
    switch(aName) {
      case SETTINGS_USB_ENABLED:
        this.handleUSBTetheringToggle(aResult);
        break;
      case SETTINGS_WIFI_ENABLED:
        this.handleWifiTetheringToggle(aResult);
        break;
      case SETTINGS_WIFI_SSID:
      case SETTINGS_WIFI_SECURITY_TYPE:
      case SETTINGS_WIFI_SECURITY_PASSWORD:
      case SETTINGS_WIFI_IP:
      case SETTINGS_WIFI_PREFIX:
      case SETTINGS_WIFI_DHCPSERVER_STARTIP:
      case SETTINGS_WIFI_DHCPSERVER_ENDIP:
      case SETTINGS_USB_IP:
      case SETTINGS_USB_PREFIX:
      case SETTINGS_USB_DHCPSERVER_STARTIP:
      case SETTINGS_USB_DHCPSERVER_ENDIP:
        this.tetheringSettings[aName] = aResult;
        debug("'" + aName + "'" + " is now " + this.tetheringSettings[aName]);
        break;
    };
  },

  handleError: function handleError(aErrorMessage) {
    debug("There was an error while reading Tethering settings.");
    this.tetheringSettings = {};
    this.tetheringSettings[SETTINGS_WIFI_ENABLED] = false;
    this.tetheringSettings[SETTINGS_USB_ENABLED] = false;
  },

  getNetworkInterface: function getNetworkInterface(type) {
    for each (let network in this.networkInterfaces) {
      if (network.type == type) {
        return network;
      }
    }
    return null;
  },

  // External and internal interface name.
  _tetheringInterface: null,

  handleUSBTetheringToggle: function handleUSBTetheringToggle(enable) {
    if (this.tetheringSettings[SETTINGS_USB_ENABLED] == enable) {
      return;
    }

    if (!enable) {
      this.tetheringSettings[SETTINGS_USB_ENABLED] = false;
      this.disableTethering(TETHERING_TYPE_USB);
      return;
    }

    if (this.active) {
      this._tetheringInterface[TETHERING_TYPE_USB].externalInterface = this.active.name
    } else {
      let mobile = this.getNetworkInterface(Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE);
      if (!mobile) {
        let params = {
          enable: enable,
          resultCode: NETD_COMMAND_ERROR,
          resultReason: "Mobile interface is not registered"
        };
        this.usbTetheringResultReport(params);
        debug("Can't enable usb tethering - no active network interface.");
        return;
      }
      this._tetheringInterface[TETHERING_TYPE_USB].externalInterface = mobile.name;
    }
    this.tetheringSettings[SETTINGS_USB_ENABLED] = true;
    this.enableTethering(TETHERING_TYPE_USB);
  },

  handleWifiTetheringToggle: function handleWifiTetheringToggle(enable) {
    if (this.tetheringSettings[SETTINGS_WIFI_ENABLED] == enable) {
      return;
    }

    let mobile = this.getNetworkInterface(Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE);
    if (!mobile) {
      let params = {
        enable: enable,
        unload: false,
        resultCode: NETD_COMMAND_ERROR,
        resultReason: "Mobile interface is not registered"
      };
      this.wifiTetheringResultReport(params);
      debug("Can't enable wifi tethering, MOBILE interface is not registered.");
      return;
    }
    this._tetheringInterface[TETHERING_TYPE_WIFI].externalInterface = mobile.name;

    if (enable) {
      this.tetheringSettings[SETTINGS_WIFI_ENABLED] = true;
      this.enableTethering(TETHERING_TYPE_WIFI);
    } else {
      this.tetheringSettings[SETTINGS_WIFI_ENABLED] = false;
      this.disableTethering(TETHERING_TYPE_WIFI);
    }
  },

  // Callback to enable Wifi tethering.
  setWifiTetheringEnabledResult: function setWifiTetheringEnabledResult(result, network) {
    if (!network || result < 0) {
      let params = {
        enable: true,
        unload: false,
        resultCode: NETD_COMMAND_ERROR,
        resultReason: "Can't enable wifi tethering"
      };
      debug("WifiWorker reports enable tethering result ..." + params.resultReason);
      this.wifiTetheringResultReport(params);
      return;
    }
    this._tetheringInterface[TETHERING_TYPE_WIFI].internalInterface = network.name;
    this.setWifiTethering(true, this._tetheringInterface[TETHERING_TYPE_WIFI]);
  },

  // Callback to involve after wifi manager unloads wifi driver.
  setWifiTetheringDisabledResult: function setWifiTetheringDisabledResult(result, network) {
    if (result < 0) {
      let params = {
        enable: false,
        unload: false,
        resultCode: NETD_COMMAND_ERROR,
        resultReason: "Can't enable wifi tethering"
      };
      debug("WifiWorker reports disable tethering result ..." + params.resultReason);
      this.wifiTetheringResultReport(params);
      return;
    }
  },

  getWifiTetheringParameters: function getWifiTetheringParameters(enable, tetheringinterface) {
    let ssid;
    let securityType;
    let securityId;
    let interfaceIp;
    let prefix;
    let dhcpStartIp;
    let dhcpEndIp;
    let internalInterface = tetheringinterface.internalInterface;
    let externalInterface = tetheringinterface.externalInterface;

    ssid = this.tetheringSettings[SETTINGS_WIFI_SSID];
    securityType = this.tetheringSettings[SETTINGS_WIFI_SECURITY_TYPE];
    securityId = this.tetheringSettings[SETTINGS_WIFI_SECURITY_PASSWORD];
    interfaceIp = this.tetheringSettings[SETTINGS_WIFI_IP];
    prefix = this.tetheringSettings[SETTINGS_WIFI_PREFIX];
    dhcpStartIp = this.tetheringSettings[SETTINGS_WIFI_DHCPSERVER_STARTIP];
    dhcpEndIp = this.tetheringSettings[SETTINGS_WIFI_DHCPSERVER_ENDIP];

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
    if (!securityId) {
      debug("Invalid security password.");
      return null;
    }
    // Using the default values here until application supports these settings.
    if (interfaceIp == "" || prefix == "" ||
        dhcpStartIp == "" || dhcpEndIp == "") {
      debug("Invalid subnet information.");
      return null;
    }

    return {
      ifname: internalInterface,
      wifictrlinterfacename: WIFI_CTRL_INTERFACE,
      ssid: ssid,
      security: securityType,
      key: securityId,
      ip: interfaceIp,
      prefix: prefix,
      startIp: dhcpStartIp,
      endIp: dhcpEndIp,
      dns1: "",
      dns2: "",
      internalIfname: internalInterface,
      externalIfname: externalInterface,
      enable: enable,
      mode: enable ? WIFI_FIRMWARE_AP : WIFI_FIRMWARE_STATION,
      link: enable ? NETWORK_INTERFACE_UP : NETWORK_INTERFACE_DOWN
    };
  },

  getUSBTetheringParameters: function getUSBTetheringParameters(enable, tetheringinterface) {
    let interfaceIp;
    let prefix;
    let dhcpStartIp;
    let dhcpEndIp;
    let internalInterface = tetheringinterface.internalInterface;
    let externalInterface = tetheringinterface.externalInterface;

    interfaceIp = this.tetheringSettings[SETTINGS_USB_IP];
    prefix = this.tetheringSettings[SETTINGS_USB_PREFIX];
    dhcpStartIp = this.tetheringSettings[SETTINGS_USB_DHCPSERVER_STARTIP];
    dhcpEndIp = this.tetheringSettings[SETTINGS_USB_DHCPSERVER_ENDIP];

    // Using the default values here until application support these settings.
    if (interfaceIp == "" || prefix == "" ||
        dhcpStartIp == "" || dhcpEndIp == "") {
      debug("Invalid subnet information.");
      return null;
    }

    return {
      ifname: internalInterface,
      ip: interfaceIp,
      prefix: prefix,
      startIp: dhcpStartIp,
      endIp: dhcpEndIp,
      dns1: "",
      dns2: "",
      internalIfname: internalInterface,
      externalIfname: externalInterface,
      enable: enable,
      link: enable ? NETWORK_INTERFACE_UP : NETWORK_INTERFACE_DOWN
    };
  },

  // Enable/disable WiFi tethering by sending commands to netd.
  setWifiTethering: function setWifiTethering(enable, tetheringInterface) {
    let params = this.getWifiTetheringParameters(enable, tetheringInterface);

    if (params === null) {
      params = {
        enable: enable,
        resultCode: NETD_COMMAND_ERROR,
        resultReason: "Invalid parameters"
      };
      (enable) ? (params.unload = true) : (params.unload = false);
      this.wifiTetheringResultReport(params);
      return;
    }
    params.cmd = "setWifiTethering";
    // The callback function in controlMessage may not be fired immediately.
    params.isAsync = true;
    this.controlMessage(params, this.wifiTetheringResultReport);
  },

  // Enable/disable USB tethering by sending commands to netd.
  setUSBTethering: function setUSBTethering(enable, tetheringInterface) {
    let params = this.getUSBTetheringParameters(enable, tetheringInterface);

    if (params === null) {
      params = {
        enable: enable,
        resultCode: NETD_COMMAND_ERROR,
        resultReason: "Invalid parameters"
      };
      this.usbTetheringResultReport(params);
      this.setUSBFunction(false, USB_FUNCTION_ADB, null);
      return;
    }

    params.cmd = "setUSBTethering";
    // The callback function in controlMessage may not be fired immediately.
    params.isAsync = true;
    this.controlMessage(params, this.usbTetheringResultReport);
  },

  // Send enable tehtering request to peripheral(WiFi/USB/Bluetooth) Manager module.
  enableTethering: function enableTethering(type) {
    if (type == TETHERING_TYPE_WIFI) {
      debug("Notify WifiWorker to help !!!");
      gWifi.setWifiTethering(true, this.setWifiTetheringEnabledResult.bind(this));
    } else if (type == TETHERING_TYPE_USB) {
      debug("Notify USB device manager to help !!!");
      this.setUSBFunction(true, USB_FUNCTION_RNDIS, this.setUSBFunctionResult);
    } else {
      debug("Fatal error !!! Unsupported type.");
    }
  },

  disableTethering: function disableTethering(type) {
    if (type == TETHERING_TYPE_WIFI) {
      this.setWifiTethering(false, this._tetheringInterface[TETHERING_TYPE_WIFI]);
    } else if (type == TETHERING_TYPE_USB) {
      this.setUSBFunction(false, USB_FUNCTION_ADB, this.setUSBFunctionResult);
    } else {
      debug("Fatal error !!! Unsupported type.");
    }
  },

  setUSBFunctionResult: function setUSBFunctionResult(data) {
    let result = data.result;
    let enable = data.enable;
    if (result) {
      this.setUSBTethering(enable, this._tetheringInterface[TETHERING_TYPE_USB]);
    } else {
      let params = {
        enable: false,
        resultCode: NETD_COMMAND_ERROR,
        resultReason: "Failed to set usb function"
      };
      this.usbTetheringResultReport(params);
      throw new Error("failed to set USB Function to adb");
    }
  },
  // Switch usb function by modifying property of persist.sys.usb.config.
  setUSBFunction: function setUSBFunction(enable, usbfunc, callback) {
    debug("Set usb function to " + usbfunc);

    let params = {
      cmd: "setUSBFunction",
      usbfunc: usbfunc,
      enable: enable
    };
    // Ask net work to report the result when this value is set to true.
    if (callback) {
      params.report = true;
    } else {
      params.report = false;
    }

    // The callback function in controlMessage may not be fired immediately.
    params.isAsync = true;
    this.controlMessage(params, callback);
  },

  wifiTetheringResultReport: function wifiTetheringResultReport(data) {
    let code = data.resultCode;
    let reason = data.resultReason;
    let enable = data.enable;
    let enableString = enable ? "Enable" : "Disable";
    let unload = data.unload;
    let settingsLock = gSettingsService.getLock();

    debug(enableString + " Wifi tethering result: Code " + code + " reason " + reason);
    // Unload wifi driver when
    // 1. We have sent disable tethering commands to netd and switch wifi to station mode.
    // 2. We fail to enable wifi tethering.
    if (unload) {
      gWifi.setWifiTethering(false, this.setWifiTetheringDisabledResult.bind(this));
    }

    // Disable tethering settings when fail to enable it.
    if (code < NETD_COMMAND_OKAY && code >= NETD_COMMAND_ERROR) {
      this.tetheringSettings[SETTINGS_WIFI_ENABLED] = false;
      settingsLock.set("tethering.wifi.enabled", false, null);
    }
  },

  usbTetheringResultReport: function usbTetheringResultReport(data) {
    let code = data.resultCode;
    let reason = data.resultReason;
    let enable = data.enable;
    let enableString = enable ? "Enable" : "Disable";
    let settingsLock = gSettingsService.getLock();

    debug(enableString + " USB tethering result: Code " + code + " reason " + reason);
    // Disable tethering settings when fail to enable it.
    if (code < NETD_COMMAND_OKAY && code >= NETD_COMMAND_ERROR) {
      this.tetheringSettings[SETTINGS_USB_ENABLED] = false;
      settingsLock.set("tethering.usb.enabled", false, null);
    }
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([NetworkManager]);


let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- NetworkManager: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
