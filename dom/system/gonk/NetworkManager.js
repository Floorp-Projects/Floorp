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

  let settingsLock = gSettingsService.createLock();
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

  this.setAndConfigureActive();

  let self = this;
  this.waitForConnectionReadyCallback = null;
  settingsLock.get(SETTINGS_WIFI_ENABLED, {
    handle: function (aName, aResult) {
      if (!aResult) {
        return;
      }
      // Turn on wifi tethering when the mobile data connection is established.
      self.waitForConnectionReadyCallback = (function callback() {
        let settingsLock = gSettingsService.createLock();
        settingsLock.set(SETTINGS_WIFI_ENABLED, aResult, null);
      });
    },

    handleError: function (aErrorMessage) {
      debug("Error reading the 'tethering.wifi.enabled' setting: " + aErrorMessage);
    }
  });
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
            // Add host route for data calls
            if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE ||
                network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS ||
                network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL) {
              this.addHostRoute(network);
            }
            // Remove pre-created default route and let setAndConfigureActive()
            // to set default route only on preferred network
            this.removeDefaultRoute(network.name);
            this.setAndConfigureActive();
            // Turn on wifi tethering when the callback is set.
            if (this.waitForConnectionReadyCallback) {
              this.waitForConnectionReadyCallback.call(this);
              this.waitForConnectionReadyCallback = null;
            }
            break;
          case Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED:
            // Remove host route for data calls
            if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE ||
                network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS ||
                network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL) {
              this.removeHostRoute(network);
            }
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
    // Add host route for data calls
    if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE ||
        network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS ||
        network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL) {
      this.addHostRoute(network);
    }
    // Remove pre-created default route and let setAndConfigureActive()
    // to set default route only on preferred network
    this.removeDefaultRoute(network.name);
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
    // Remove host route for data calls
    if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE ||
        network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS ||
        network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL) {
      this.removeHostRoute(network);
    }
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
         Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS,
         Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL].indexOf(val) == -1) {
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

  getNetworkInterfaceStats: function getNetworkInterfaceStats(connectionType, callback) {
    let iface = this.getNetworkInterface(connectionType);

    if (!iface) {
      debug("There is no interface registered for network type " + connectionType);
      return false;
    }

    debug("getNetworkInterfaceStats for " + iface.name);

    let params = {
      cmd: "getNetworkInterfaceStats",
      ifname: iface.name,
      connType: connectionType
    };

    params.report = true;
    params.isAsync = true;

    this.controlMessage(params, function(result) {
      let success = result.resultCode >= NETD_COMMAND_OKAY && result.resultCode < NETD_COMMAND_ERROR;
      callback.networkStatsAvailable(success, result.connType, result.rxBytes, result.txBytes, result.date);
    });

    return true;
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
    }
    debug("NetworkManager received message from worker: " + JSON.stringify(e));
  },

  /**
   * Determine the active interface and configure it.
   */
  setAndConfigureActive: function setAndConfigureActive() {
    debug("Evaluating whether active network needs to be changed.");
    let oldActive = this.active;
    let defaultDataNetwork;

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
      if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE) {
        defaultDataNetwork = network;
      }
      this.active = network;
      if (network.type == this.preferredNetworkType) {
        debug("Found our preferred type of network: " + network.name);
        break;
      }
    }
    if (this.active) {
      // Give higher priority to default data APN than seconary APN.
      // If default data APN is not connected, we still set default route
      // and DNS on seconary APN.
      if (defaultDataNetwork &&
          (this.active.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS ||
           this.active.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL) &&
          this.active.type != this.preferredNetworkType) {
        this.active = defaultDataNetwork;
      }
      this.setDefaultRouteAndDNS(oldActive);
    }
    Services.io.offline = !this.active;
  },

  setDefaultRouteAndDNS: function setDefaultRouteAndDNS(oldInterface) {
    debug("Going to change route and DNS to " + this.active.name);
    let options = {
      cmd: this.active.dhcp ? "runDHCPAndSetDefaultRouteAndDNS" : "setDefaultRouteAndDNS",
      ifname: this.active.name,
      oldIfname: (oldInterface && oldInterface != this.active) ? oldInterface.name : null,
      gateway_str: this.active.gateway,
      dns1_str: this.active.dns1,
      dns2_str: this.active.dns2
    };
    this.worker.postMessage(options);
    this.setNetworkProxy();
  },

  removeDefaultRoute: function removeDefaultRoute(ifname) {
    debug("Remove default route for " + ifname);
    let options = {
      cmd: "removeDefaultRoute",
      ifname: ifname
    }
    this.worker.postMessage(options);
  },

  addHostRoute: function addHostRoute(network) {
    debug("Going to add host route on " + network.name);
    let options = {
      cmd: "addHostRoute",
      ifname: network.name,
      dns1: network.dns1,
      dns2: network.dns2,
      gateway: network.gateway,
      httpproxy: network.httpProxyHost,
      mmsproxy: Services.prefs.getCharPref("ril.mms.mmsproxy")
    };
    this.worker.postMessage(options);
  },

  removeHostRoute: function removeHostRoute(network) {
    debug("Going to remove host route on " + network.name);
    let options = {
      cmd: "removeHostRoute",
      ifname: network.name,
      dns1: network.dns1,
      dns2: network.dns2,
      gateway: network.gateway,
      httpproxy: network.httpProxyHost,
      mmsproxy: Services.prefs.getCharPref("ril.mms.mmsproxy")
    };
    this.worker.postMessage(options);
  },

  setNetworkProxy: function setNetworkProxy() {
    try {
      if (!this.active.httpProxyHost || this.active.httpProxyHost == "") {
        // Sets direct connection to internet.
        Services.prefs.clearUserPref("network.proxy.type");
        Services.prefs.clearUserPref("network.proxy.share_proxy_settings");
        Services.prefs.clearUserPref("network.proxy.http");
        Services.prefs.clearUserPref("network.proxy.http_port");
        Services.prefs.clearUserPref("network.proxy.ssl");
        Services.prefs.clearUserPref("network.proxy.ssl_port");
        debug("No proxy support for " + this.active.name + " network interface.");
        return;
      }

      debug("Going to set proxy settings for " + this.active.name + " network interface.");
      // Sets manual proxy configuration.
      Services.prefs.setIntPref("network.proxy.type", MANUAL_PROXY_CONFIGURATION);
      // Do not use this proxy server for all protocols.
      Services.prefs.setBoolPref("network.proxy.share_proxy_settings", false);
      Services.prefs.setCharPref("network.proxy.http", this.active.httpProxyHost);
      Services.prefs.setCharPref("network.proxy.ssl", this.active.httpProxyHost);
      let port = this.active.httpProxyPort == "" ? 8080 : this.active.httpProxyPort;
      Services.prefs.setIntPref("network.proxy.http_port", port);
      Services.prefs.setIntPref("network.proxy.ssl_port", port);
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
      // SETTINGS_WIFI_ENABLED is handled in WifiManager.js to deal with
      // the interaction between wifi and wifi tethering settings. Also, we
      // update tetheringSettings[SETTINGS_WIFI_ENABLED] in setWifiTethering
      // function.
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
      this.setUSBFunction(false, USB_FUNCTION_ADB, this.setUSBFunctionResult);
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
    this.setUSBFunction(true, USB_FUNCTION_RNDIS, this.setUSBFunctionResult);
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
    if (securityType != WIFI_SECURITY_TYPE_NONE && !securityId) {
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

  get wifiTetheringEnabled() {
    return this.tetheringSettings[SETTINGS_WIFI_ENABLED];
  },

  notifyError: function notifyError(resetSettings, callback, msg) {
    if (resetSettings) {
      let settingsLock = gSettingsService.createLock();
      this.tetheringSettings[SETTINGS_WIFI_ENABLED] = false;
      // Disable wifi tethering with a useful error message for the user.
      settingsLock.set("tethering.wifi.enabled", false, null, msg);
    }

    debug("setWifiTethering: " + (msg ? msg : "success"));

    if (callback) {
      callback.wifiTetheringEnabledChange(msg);
    }
  },

  // Enable/disable WiFi tethering by sending commands to netd.
  setWifiTethering: function setWifiTethering(enable, network, callback) {
    if (this.tetheringSettings[SETTINGS_WIFI_ENABLED] == enable) {
      this.notifyError(false, callback, "no change");
      return;
    }

    if (!network) {
      this.notifyError(true, callback, "invalid network information");
      return;
    }
    this._tetheringInterface[TETHERING_TYPE_WIFI].internalInterface = network.name;

    let mobile = this.getNetworkInterface(Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE);
    if (!mobile) {
      this.notifyError(true, callback, "mobile interface is not registered");
      return;
    }
    // Clear this flag to prevent unexpected action.
    this.waitForConnectionReadyCallback = null;

    this._tetheringInterface[TETHERING_TYPE_WIFI].externalInterface = mobile.name;

    let params = this.getWifiTetheringParameters(enable, this._tetheringInterface[TETHERING_TYPE_WIFI]);
    if (!params) {
      this.notifyError(true, callback, "invalid parameters");
      return;
    }

    params.cmd = "setWifiTethering";
    // The callback function in controlMessage may not be fired immediately.
    params.isAsync = true;
    this.controlMessage(params, function setWifiTetheringResult(data) {
      let code = data.resultCode;
      let reason = data.resultReason;
      let enable = data.enable;
      let enableString = enable ? "Enable" : "Disable";

      debug(enableString + " Wifi tethering result: Code " + code + " reason " + reason);
      // Update settings status.
      this.tetheringSettings[SETTINGS_WIFI_ENABLED] = enable;

      if (isError(code)) {
        this.notifyError(true, callback, "netd command error");
      } else {
        this.notifyError(false, callback, null);
      }
    }.bind(this));
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

  usbTetheringResultReport: function usbTetheringResultReport(data) {
    let code = data.resultCode;
    let reason = data.resultReason;
    let enable = data.enable;
    let enableString = enable ? "Enable" : "Disable";
    let settingsLock = gSettingsService.createLock();

    debug(enableString + " USB tethering result: Code " + code + " reason " + reason);
    // Disable tethering settings when fail to enable it.
    if (isError(code)) {
      this.tetheringSettings[SETTINGS_USB_ENABLED] = false;
      settingsLock.set("tethering.usb.enabled", false, null);
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NetworkManager]);


let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- NetworkManager: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
