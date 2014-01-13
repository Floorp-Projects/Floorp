/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

const NETWORKMANAGER_CONTRACTID = "@mozilla.org/network/manager;1";
const NETWORKMANAGER_CID =
  Components.ID("{33901e46-33b8-11e1-9869-f46d04d25bcc}");

const DEFAULT_PREFERRED_NETWORK_TYPE = Ci.nsINetworkInterface.NETWORK_TYPE_WIFI;

XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");
XPCOMUtils.defineLazyGetter(this, "ppmm", function() {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"]
         .getService(Ci.nsIMessageBroadcaster);
});

XPCOMUtils.defineLazyServiceGetter(this, "gDNSService",
                                   "@mozilla.org/network/dns-service;1",
                                   "nsIDNSService");

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkService",
                                   "@mozilla.org/network/service;1",
                                   "nsINetworkService");

const TOPIC_INTERFACE_STATE_CHANGED  = "network-interface-state-changed";
const TOPIC_INTERFACE_REGISTERED     = "network-interface-registered";
const TOPIC_INTERFACE_UNREGISTERED   = "network-interface-unregistered";
const TOPIC_ACTIVE_CHANGED           = "network-active-changed";
const TOPIC_MOZSETTINGS_CHANGED      = "mozsettings-changed";
const TOPIC_PREF_CHANGED             = "nsPref:changed";
const TOPIC_XPCOM_SHUTDOWN           = "xpcom-shutdown";
const PREF_MANAGE_OFFLINE_STATUS     = "network.gonk.manage-offline-status";

const POSSIBLE_USB_INTERFACE_NAME = "rndis0,usb0";
const DEFAULT_USB_INTERFACE_NAME  = "rndis0";
const DEFAULT_3G_INTERFACE_NAME   = "rmnet0";
const DEFAULT_WIFI_INTERFACE_NAME = "wlan0";

// The kernel's proc entry for network lists.
const KERNEL_NETWORK_ENTRY = "/sys/class/net";

const TETHERING_TYPE_WIFI = "WiFi";
const TETHERING_TYPE_USB  = "USB";

const WIFI_FIRMWARE_AP            = "AP";
const WIFI_FIRMWARE_STATION       = "STA";
const WIFI_SECURITY_TYPE_NONE     = "open";
const WIFI_SECURITY_TYPE_WPA_PSK  = "wpa-psk";
const WIFI_SECURITY_TYPE_WPA2_PSK = "wpa2-psk";
const WIFI_CTRL_INTERFACE         = "wl0.1";

const NETWORK_INTERFACE_UP   = "up";
const NETWORK_INTERFACE_DOWN = "down";

const TETHERING_STATE_ONGOING = "ongoing";
const TETHERING_STATE_IDLE    = "idle";

// Settings DB path for USB tethering.
const SETTINGS_USB_ENABLED             = "tethering.usb.enabled";
const SETTINGS_USB_IP                  = "tethering.usb.ip";
const SETTINGS_USB_PREFIX              = "tethering.usb.prefix";
const SETTINGS_USB_DHCPSERVER_STARTIP  = "tethering.usb.dhcpserver.startip";
const SETTINGS_USB_DHCPSERVER_ENDIP    = "tethering.usb.dhcpserver.endip";
const SETTINGS_USB_DNS1                = "tethering.usb.dns1";
const SETTINGS_USB_DNS2                = "tethering.usb.dns2";

// Settings DB path for WIFI tethering.
const SETTINGS_WIFI_DHCPSERVER_STARTIP = "tethering.wifi.dhcpserver.startip";
const SETTINGS_WIFI_DHCPSERVER_ENDIP   = "tethering.wifi.dhcpserver.endip";

// Default value for USB tethering.
const DEFAULT_USB_IP                   = "192.168.0.1";
const DEFAULT_USB_PREFIX               = "24";
const DEFAULT_USB_DHCPSERVER_STARTIP   = "192.168.0.10";
const DEFAULT_USB_DHCPSERVER_ENDIP     = "192.168.0.30";

const DEFAULT_DNS1                     = "8.8.8.8";
const DEFAULT_DNS2                     = "8.8.4.4";

const DEFAULT_WIFI_DHCPSERVER_STARTIP  = "192.168.1.10";
const DEFAULT_WIFI_DHCPSERVER_ENDIP    = "192.168.1.30";

const DEBUG = false;

function defineLazyRegExp(obj, name, pattern) {
  obj.__defineGetter__(name, function() {
    delete obj[name];
    return obj[name] = new RegExp(pattern);
  });
}

/**
 * This component watches for network interfaces changing state and then
 * adjusts routes etc. accordingly.
 */
function NetworkManager() {
  this.networkInterfaces = {};
  Services.obs.addObserver(this, TOPIC_INTERFACE_STATE_CHANGED, true);
#ifdef MOZ_B2G_RIL
  Services.obs.addObserver(this, TOPIC_INTERFACE_REGISTERED, true);
  Services.obs.addObserver(this, TOPIC_INTERFACE_UNREGISTERED, true);
#endif
  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN, false);
  Services.obs.addObserver(this, TOPIC_MOZSETTINGS_CHANGED, false);

  try {
    this._manageOfflineStatus =
      Services.prefs.getBoolPref(PREF_MANAGE_OFFLINE_STATUS);
  } catch(ex) {
    // Ignore.
  }
  Services.prefs.addObserver(PREF_MANAGE_OFFLINE_STATUS, this, false);

  // Possible usb tethering interfaces for different gonk platform.
  this.possibleInterface = POSSIBLE_USB_INTERFACE_NAME.split(",");

  // Default values for internal and external interfaces.
  this._tetheringInterface = Object.create(null);
  this._tetheringInterface[TETHERING_TYPE_USB] = {
    externalInterface: DEFAULT_3G_INTERFACE_NAME,
    internalInterface: DEFAULT_USB_INTERFACE_NAME
  };
  this._tetheringInterface[TETHERING_TYPE_WIFI] = {
    externalInterface: DEFAULT_3G_INTERFACE_NAME,
    internalInterface: DEFAULT_WIFI_INTERFACE_NAME
  };

  this.initTetheringSettings();

  let settingsLock = gSettingsService.createLock();
  // Read usb tethering data from settings DB.
  settingsLock.get(SETTINGS_USB_IP, this);
  settingsLock.get(SETTINGS_USB_PREFIX, this);
  settingsLock.get(SETTINGS_USB_DHCPSERVER_STARTIP, this);
  settingsLock.get(SETTINGS_USB_DHCPSERVER_ENDIP, this);
  settingsLock.get(SETTINGS_USB_DNS1, this);
  settingsLock.get(SETTINGS_USB_DNS2, this);
  settingsLock.get(SETTINGS_USB_ENABLED, this);

  // Read wifi tethering data from settings DB.
  settingsLock.get(SETTINGS_WIFI_DHCPSERVER_STARTIP, this);
  settingsLock.get(SETTINGS_WIFI_DHCPSERVER_ENDIP, this);

  this._usbTetheringSettingsToRead = [SETTINGS_USB_IP,
                                      SETTINGS_USB_PREFIX,
                                      SETTINGS_USB_DHCPSERVER_STARTIP,
                                      SETTINGS_USB_DHCPSERVER_ENDIP,
                                      SETTINGS_USB_DNS1,
                                      SETTINGS_USB_DNS2,
                                      SETTINGS_USB_ENABLED,
                                      SETTINGS_WIFI_DHCPSERVER_STARTIP,
                                      SETTINGS_WIFI_DHCPSERVER_ENDIP];

  this.wantConnectionEvent = null;
  this.setAndConfigureActive();

  ppmm.addMessageListener('NetworkInterfaceList:ListInterface', this);

  // Used in resolveHostname().
  defineLazyRegExp(this, "REGEXP_IPV4", "^\\d{1,3}(?:\\.\\d{1,3}){3}$");
  defineLazyRegExp(this, "REGEXP_IPV6", "^[\\da-fA-F]{4}(?::[\\da-fA-F]{4}){7}$");
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
                                         Ci.nsISettingsServiceCallback]),

  // nsIObserver

  observe: function(subject, topic, data) {
    switch (topic) {
      case TOPIC_INTERFACE_STATE_CHANGED:
        let network = subject.QueryInterface(Ci.nsINetworkInterface);
        debug("Network " + network.name + " changed state to " + network.state);
        switch (network.state) {
          case Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED:
#ifdef MOZ_B2G_RIL
            // Add host route for data calls
            if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE ||
                network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS ||
                network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL) {
              gNetworkService.removeHostRoutes(network.name);
              gNetworkService.addHostRoute(network);
            }
            // Add extra host route. For example, mms proxy or mmsc.
            this.setExtraHostRoute(network);
#endif
            // Remove pre-created default route and let setAndConfigureActive()
            // to set default route only on preferred network
            gNetworkService.removeDefaultRoute(network.name);
            this.setAndConfigureActive();
#ifdef MOZ_B2G_RIL
            // Update data connection when Wifi connected/disconnected
            if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_WIFI) {
              for (let i = 0; i < this.mRil.numRadioInterfaces; i++) {
                this.mRil.getRadioInterface(i).updateRILNetworkInterface();
              }
            }
#endif

            this.onConnectionChanged(network);

            // Probing the public network accessibility after routing table is ready
            CaptivePortalDetectionHelper
              .notify(CaptivePortalDetectionHelper.EVENT_CONNECT, this.active);
            break;
          case Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED:
#ifdef MOZ_B2G_RIL
            // Remove host route for data calls
            if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE ||
                network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS ||
                network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL) {
              gNetworkService.removeHostRoute(network);
            }
            // Remove extra host route. For example, mms proxy or mmsc.
            this.removeExtraHostRoute(network);
#endif
            // Remove routing table in /proc/net/route
            if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_WIFI) {
              gNetworkService.resetRoutingTable(network);
#ifdef MOZ_B2G_RIL
            } else if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE) {
              gNetworkService.removeDefaultRoute(network.name);
#endif
            }

            // Abort ongoing captive portal detection on the wifi interface
            CaptivePortalDetectionHelper
              .notify(CaptivePortalDetectionHelper.EVENT_DISCONNECT, network);
            this.setAndConfigureActive();
#ifdef MOZ_B2G_RIL
            // Update data connection when Wifi connected/disconnected
            if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_WIFI) {
              for (let i = 0; i < this.mRil.numRadioInterfaces; i++) {
                this.mRil.getRadioInterface(i).updateRILNetworkInterface();
              }
            }
#endif
            break;
        }
        break;
#ifdef MOZ_B2G_RIL
      case TOPIC_INTERFACE_REGISTERED:
        let regNetwork = subject.QueryInterface(Ci.nsINetworkInterface);
        // Add extra host route. For example, mms proxy or mmsc.
        this.setExtraHostRoute(regNetwork);
        break;
      case TOPIC_INTERFACE_UNREGISTERED:
        let unregNetwork = subject.QueryInterface(Ci.nsINetworkInterface);
        // Remove extra host route. For example, mms proxy or mmsc.
        this.removeExtraHostRoute(unregNetwork);
        break;
#endif
      case TOPIC_MOZSETTINGS_CHANGED:
        let setting = JSON.parse(data);
        this.handle(setting.key, setting.value);
        break;
      case TOPIC_PREF_CHANGED:
        this._manageOfflineStatus =
          Services.prefs.getBoolPref(PREF_MANAGE_OFFLINE_STATUS);
        debug(PREF_MANAGE_OFFLINE_STATUS + " has changed to " + this._manageOfflineStatus);
        break;
      case TOPIC_XPCOM_SHUTDOWN:
        Services.obs.removeObserver(this, TOPIC_XPCOM_SHUTDOWN);
        Services.obs.removeObserver(this, TOPIC_MOZSETTINGS_CHANGED);
#ifdef MOZ_B2G_RIL
        Services.obs.removeObserver(this, TOPIC_INTERFACE_REGISTERED);
        Services.obs.removeObserver(this, TOPIC_INTERFACE_UNREGISTERED);
#endif
        Services.obs.removeObserver(this, TOPIC_INTERFACE_STATE_CHANGED);
        break;
    }
  },

  receiveMessage: function(aMsg) {
    switch (aMsg.name) {
      case "NetworkInterfaceList:ListInterface": {
#ifdef MOZ_B2G_RIL
        let excludeMms = aMsg.json.exculdeMms;
        let excludeSupl = aMsg.json.exculdeSupl;
#endif
        let interfaces = [];

        for each (let i in this.networkInterfaces) {
#ifdef MOZ_B2G_RIL
          if ((i.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS && excludeMms) ||
              (i.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL && excludeSupl)) {
            continue;
          }
#endif
          interfaces.push({
            state: i.state,
            type: i.type,
            name: i.name,
            ip: i.ip,
            netmask: i.netmask,
            broadcast: i.broadcast,
            gateway: i.gateway,
            dns1: i.dns1,
            dns2: i.dns2,
            httpProxyHost: i.httpProxyHost,
            httpProxyPort: i.httpProxyPort
          });
        }
        return interfaces;
      }
    }
  },

  // nsINetworkManager

  registerNetworkInterface: function(network) {
    if (!(network instanceof Ci.nsINetworkInterface)) {
      throw Components.Exception("Argument must be nsINetworkInterface.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    if (network.name in this.networkInterfaces) {
      throw Components.Exception("Network with that name already registered!",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    this.networkInterfaces[network.name] = network;
#ifdef MOZ_B2G_RIL
    // Add host route for data calls
    if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE ||
        network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS ||
        network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL) {
      gNetworkService.addHostRoute(network);
    }
#endif
    // Remove pre-created default route and let setAndConfigureActive()
    // to set default route only on preferred network
    gNetworkService.removeDefaultRoute(network.name);
    this.setAndConfigureActive();
    Services.obs.notifyObservers(network, TOPIC_INTERFACE_REGISTERED, null);
    debug("Network '" + network.name + "' registered.");
  },

  unregisterNetworkInterface: function(network) {
    if (!(network instanceof Ci.nsINetworkInterface)) {
      throw Components.Exception("Argument must be nsINetworkInterface.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    if (!(network.name in this.networkInterfaces)) {
      throw Components.Exception("No network with that name registered.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    delete this.networkInterfaces[network.name];
#ifdef MOZ_B2G_RIL
    // Remove host route for data calls
    if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE ||
        network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS ||
        network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL) {
      gNetworkService.removeHostRoute(network);
    }
#endif
    this.setAndConfigureActive();
    Services.obs.notifyObservers(network, TOPIC_INTERFACE_UNREGISTERED, null);
    debug("Network '" + network.name + "' unregistered.");
  },

  _manageOfflineStatus: true,

  networkInterfaces: null,

  _preferredNetworkType: DEFAULT_PREFERRED_NETWORK_TYPE,
  get preferredNetworkType() {
    return this._preferredNetworkType;
  },
  set preferredNetworkType(val) {
#ifdef MOZ_B2G_RIL
    if ([Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
         Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE].indexOf(val) == -1) {
#else
    if (val != Ci.nsINetworkInterface.NETWORK_TYPE_WIFI) {
#endif
      throw "Invalid network type";
    }
    this._preferredNetworkType = val;
  },

  active: null,
  _overriddenActive: null,

  // Clone network info so we can still get information when network is disconnected
  _activeInfo: null,

  overrideActive: function(network) {
#ifdef MOZ_B2G_RIL
    if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS ||
        network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL) {
      throw "Invalid network type";
    }
#endif
    this._overriddenActive = network;
    this.setAndConfigureActive();
  },

#ifdef MOZ_B2G_RIL
  setExtraHostRoute: function(network) {
    if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS) {
      if (!(network instanceof Ci.nsIRilNetworkInterface)) {
        debug("Network for MMS must be an instance of nsIRilNetworkInterface");
        return;
      }

      network = network.QueryInterface(Ci.nsIRilNetworkInterface);

      debug("Network '" + network.name + "' registered, " +
            "adding mmsproxy and/or mmsc route");

      let mmsHosts = this.resolveHostname([network.mmsProxy, network.mmsc]);
      if (mmsHosts.length == 0) {
        debug("No valid hostnames can be added. Stop adding host route.");
        return;
      }

      gNetworkService.addHostRouteWithResolve(network, mmsHosts);
    }
  },

  removeExtraHostRoute: function(network) {
    if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS) {
      if (!(network instanceof Ci.nsIRilNetworkInterface)) {
        debug("Network for MMS must be an instance of nsIRilNetworkInterface");
        return;
      }

      network = network.QueryInterface(Ci.nsIRilNetworkInterface);

      debug("Network '" + network.name + "' unregistered, " +
            "removing mmsproxy and/or mmsc route");

      let mmsHosts = this.resolveHostname([network.mmsProxy, network.mmsc]);
      if (mmsHosts.length == 0) {
        debug("No valid hostnames can be removed. Stop removing host route.");
        return;
      }

      gNetworkService.removeHostRouteWithResolve(network, mmsHosts);
    }
  },
#endif // MOZ_B2G_RIL

  /**
   * Determine the active interface and configure it.
   */
  setAndConfigureActive: function() {
    debug("Evaluating whether active network needs to be changed.");
    let oldActive = this.active;

    if (this._overriddenActive) {
      debug("We have an override for the active network: " +
            this._overriddenActive.name);
      // The override was just set, so reconfigure the network.
      if (this.active != this._overriddenActive) {
        this.active = this._overriddenActive;
        gNetworkService.setDefaultRouteAndDNS(this.active, oldActive);
        Services.obs.notifyObservers(this.active, TOPIC_ACTIVE_CHANGED, null);
      }
      return;
    }

    // The active network is already our preferred type.
    if (this.active &&
        this.active.state == Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED &&
        this.active.type == this._preferredNetworkType) {
      debug("Active network is already our preferred type.");
      gNetworkService.setDefaultRouteAndDNS(this.active, oldActive);
      return;
    }

    // Find a suitable network interface to activate.
    this.active = null;
    this._activeInfo = Object.create(null);
#ifdef MOZ_B2G_RIL
    let defaultDataNetwork;
#endif
    for each (let network in this.networkInterfaces) {
      if (network.state != Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED) {
        continue;
      }
#ifdef MOZ_B2G_RIL
      if (network.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE) {
        defaultDataNetwork = network;
      }
#endif
      this.active = network;
      this._activeInfo = {name:network.name, ip:network.ip, netmask:network.netmask};
      if (network.type == this.preferredNetworkType) {
        debug("Found our preferred type of network: " + network.name);
        break;
      }
    }
    if (this.active) {
#ifdef MOZ_B2G_RIL
      // Give higher priority to default data APN than seconary APN.
      // If default data APN is not connected, we still set default route
      // and DNS on seconary APN.
      if (defaultDataNetwork &&
          (this.active.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS ||
           this.active.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL) &&
          this.active.type != this.preferredNetworkType) {
        this.active = defaultDataNetwork;
      }
      // Don't set default route on secondary APN
      if (this.active.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS ||
          this.active.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL) {
        gNetworkService.setDNS(this.active);
      } else {
#endif // MOZ_B2G_RIL
        gNetworkService.setDefaultRouteAndDNS(this.active, oldActive);
#ifdef MOZ_B2G_RIL
      }
#endif
      if (this.active != oldActive) {
        Services.obs.notifyObservers(this.active, TOPIC_ACTIVE_CHANGED, null);
      }
    }

    if (this._manageOfflineStatus) {
      Services.io.offline = !this.active;
    }
  },

#ifdef MOZ_B2G_RIL
  resolveHostname: function(hosts) {
    let retval = [];

    for (let hostname of hosts) {
      // Sanity check for null, undefined and empty string... etc.
      if (!hostname) {
        continue;
      }

      try {
        let uri = Services.io.newURI(hostname, null, null);
        hostname = uri.host;
      } catch (e) {}

      // An extra check for hostnames that cannot be made by newURI(...).
      // For example, an IP address like "10.1.1.1".
      if (hostname.match(this.REGEXP_IPV4) ||
          hostname.match(this.REGEXP_IPV6)) {
        retval.push(hostname);
        continue;
      }

      try {
        let hostnameIps = gDNSService.resolve(hostname, 0);
        while (hostnameIps.hasMore()) {
          retval.push(hostnameIps.getNextAddrAsString());
          debug("Found IP at: " + JSON.stringify(retval));
        }
      } catch (e) {}
    }

    return retval;
  },
#endif

  // nsISettingsServiceCallback

  tetheringSettings: {},

  initTetheringSettings: function() {
    this.tetheringSettings[SETTINGS_USB_ENABLED] = false;
    this.tetheringSettings[SETTINGS_USB_IP] = DEFAULT_USB_IP;
    this.tetheringSettings[SETTINGS_USB_PREFIX] = DEFAULT_USB_PREFIX;
    this.tetheringSettings[SETTINGS_USB_DHCPSERVER_STARTIP] = DEFAULT_USB_DHCPSERVER_STARTIP;
    this.tetheringSettings[SETTINGS_USB_DHCPSERVER_ENDIP] = DEFAULT_USB_DHCPSERVER_ENDIP;
    this.tetheringSettings[SETTINGS_USB_DNS1] = DEFAULT_DNS1;
    this.tetheringSettings[SETTINGS_USB_DNS2] = DEFAULT_DNS2;

    this.tetheringSettings[SETTINGS_WIFI_DHCPSERVER_STARTIP] = DEFAULT_WIFI_DHCPSERVER_STARTIP;
    this.tetheringSettings[SETTINGS_WIFI_DHCPSERVER_ENDIP]   = DEFAULT_WIFI_DHCPSERVER_ENDIP;
  },

  _requestCount: 0,

  handle: function(aName, aResult) {
    switch(aName) {
      case SETTINGS_USB_ENABLED:
        this._oldUsbTetheringEnabledState = this.tetheringSettings[SETTINGS_USB_ENABLED];
      case SETTINGS_USB_IP:
      case SETTINGS_USB_PREFIX:
      case SETTINGS_USB_DHCPSERVER_STARTIP:
      case SETTINGS_USB_DHCPSERVER_ENDIP:
      case SETTINGS_USB_DNS1:
      case SETTINGS_USB_DNS2:
      case SETTINGS_WIFI_DHCPSERVER_STARTIP:
      case SETTINGS_WIFI_DHCPSERVER_ENDIP:
        if (aResult !== null) {
          this.tetheringSettings[aName] = aResult;
        }
        debug("'" + aName + "'" + " is now " + this.tetheringSettings[aName]);
        let index = this._usbTetheringSettingsToRead.indexOf(aName);

        if (index != -1) {
          this._usbTetheringSettingsToRead.splice(index, 1);
        }

        if (this._usbTetheringSettingsToRead.length) {
          debug("We haven't read completely the usb Tethering data from settings db.");
          break;
        }

        if (this._oldUsbTetheringEnabledState === this.tetheringSettings[SETTINGS_USB_ENABLED]) {
          debug("No changes for SETTINGS_USB_ENABLED flag. Nothing to do.");
          break;
        }

        this._requestCount++;
        if (this._requestCount === 1) {
          this.handleUSBTetheringToggle(aResult);
        }
        break;
    };
  },

  handleError: function(aErrorMessage) {
    debug("There was an error while reading Tethering settings.");
    this.tetheringSettings = {};
    this.tetheringSettings[SETTINGS_USB_ENABLED] = false;
  },

  getNetworkInterface: function(type) {
    for each (let network in this.networkInterfaces) {
      if (network.type == type) {
        return network;
      }
    }
    return null;
  },

  _usbTetheringAction: TETHERING_STATE_IDLE,

  _usbTetheringSettingsToRead: [],

  _oldUsbTetheringEnabledState: null,

  // External and internal interface name.
  _tetheringInterface: null,

  handleLastRequest: function() {
    let count = this._requestCount;
    this._requestCount = 0;

    if (count === 1) {
      if (this.wantConnectionEvent) {
        if (this.tetheringSettings[SETTINGS_USB_ENABLED]) {
          this.wantConnectionEvent.call(this);
        }
        this.wantConnectionEvent = null;
      }
      return;
    }

    if (count > 1) {
      this.handleUSBTetheringToggle(this.tetheringSettings[SETTINGS_USB_ENABLED]);
      this.wantConnectionEvent = null;
    }
  },

  handleUSBTetheringToggle: function(enable) {
    if (!enable) {
      this.tetheringSettings[SETTINGS_USB_ENABLED] = false;
      gNetworkService.enableUsbRndis(false, this.enableUsbRndisResult.bind(this));
      return;
    }

    if (this.active) {
      this._tetheringInterface[TETHERING_TYPE_USB].externalInterface = this.active.name
    } else {
      let mobile = this.getNetworkInterface(Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE);
      if (mobile) {
        this._tetheringInterface[TETHERING_TYPE_USB].externalInterface = mobile.name;
      }
    }
    this.tetheringSettings[SETTINGS_USB_ENABLED] = true;
    gNetworkService.enableUsbRndis(true, this.enableUsbRndisResult.bind(this));
  },

  getUSBTetheringParameters: function(enable, tetheringinterface) {
    let interfaceIp;
    let prefix;
    let wifiDhcpStartIp;
    let wifiDhcpEndIp;
    let usbDhcpStartIp;
    let usbDhcpEndIp;
    let dns1;
    let dns2;
    let internalInterface = tetheringinterface.internalInterface;
    let externalInterface = tetheringinterface.externalInterface;

    interfaceIp = this.tetheringSettings[SETTINGS_USB_IP];
    prefix = this.tetheringSettings[SETTINGS_USB_PREFIX];
    wifiDhcpStartIp = this.tetheringSettings[SETTINGS_WIFI_DHCPSERVER_STARTIP];
    wifiDhcpEndIp = this.tetheringSettings[SETTINGS_WIFI_DHCPSERVER_ENDIP];
    usbDhcpStartIp = this.tetheringSettings[SETTINGS_USB_DHCPSERVER_STARTIP];
    usbDhcpEndIp = this.tetheringSettings[SETTINGS_USB_DHCPSERVER_ENDIP];
    dns1 = this.tetheringSettings[SETTINGS_USB_DNS1];
    dns2 = this.tetheringSettings[SETTINGS_USB_DNS2];

    // Using the default values here until application support these settings.
    if (interfaceIp == "" || prefix == "" ||
        wifiDhcpStartIp == "" || wifiDhcpEndIp == "" ||
        usbDhcpStartIp == "" || usbDhcpEndIp == "") {
      debug("Invalid subnet information.");
      return null;
    }

    return {
      ifname: internalInterface,
      ip: interfaceIp,
      prefix: prefix,
      wifiStartIp: wifiDhcpStartIp,
      wifiEndIp: wifiDhcpEndIp,
      usbStartIp: usbDhcpStartIp,
      usbEndIp: usbDhcpEndIp,
      dns1: dns1,
      dns2: dns2,
      internalIfname: internalInterface,
      externalIfname: externalInterface,
      enable: enable,
      link: enable ? NETWORK_INTERFACE_UP : NETWORK_INTERFACE_DOWN
    };
  },

  notifyError: function(resetSettings, callback, msg) {
    if (resetSettings) {
      let settingsLock = gSettingsService.createLock();
      // Disable wifi tethering with a useful error message for the user.
      settingsLock.set("tethering.wifi.enabled", false, null, msg);
    }

    debug("setWifiTethering: " + (msg ? msg : "success"));

    if (callback) {
      callback.wifiTetheringEnabledChange(msg);
    }
  },

  // Enable/disable WiFi tethering by sending commands to netd.
  setWifiTethering: function(enable, network, config, callback) {
    if (!network) {
      this.notifyError(true, callback, "invalid network information");
      return;
    }

    if (!config) {
      this.notifyError(true, callback, "invalid configuration");
      return;
    }

    this._tetheringInterface[TETHERING_TYPE_WIFI].internalInterface = network.name;

    let mobile = this.getNetworkInterface(Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE);
    // Update the real interface name
    if (mobile) {
      this._tetheringInterface[TETHERING_TYPE_WIFI].externalInterface = mobile.name;
    }

    config.ifname         = this._tetheringInterface[TETHERING_TYPE_WIFI].internalInterface;
    config.internalIfname = this._tetheringInterface[TETHERING_TYPE_WIFI].internalInterface;
    config.externalIfname = this._tetheringInterface[TETHERING_TYPE_WIFI].externalInterface;

    gNetworkService.setWifiTethering(enable, config, (function(error) {
      let resetSettings = error;
      this.notifyError(resetSettings, callback, error);
    }).bind(this));
  },

  // Enable/disable USB tethering by sending commands to netd.
  setUSBTethering: function(enable,
                                            tetheringInterface,
                                            callback) {
    let params = this.getUSBTetheringParameters(enable, tetheringInterface);

    if (params === null) {
      gNetworkService.enableUsbRndis(false, function() {
        this.usbTetheringResultReport("Invalid parameters");
      });
      return;
    }

    gNetworkService.setUSBTethering(enable, params, callback);
  },

  getUsbInterface: function() {
    // Find the rndis interface.
    for (let i = 0; i < this.possibleInterface.length; i++) {
      try {
        let file = new FileUtils.File(KERNEL_NETWORK_ENTRY + "/" +
                                      this.possibleInterface[i]);
        if (file.exists()) {
          return this.possibleInterface[i];
        }
      } catch (e) {
        debug("Not " + this.possibleInterface[i] + " interface.");
      }
    }
    debug("Can't find rndis interface in possible lists.");
    return DEFAULT_USB_INTERFACE_NAME;
  },

  enableUsbRndisResult: function(success, enable) {
    if (success) {
      this._tetheringInterface[TETHERING_TYPE_USB].internalInterface = this.getUsbInterface();
      this.setUSBTethering(enable,
                           this._tetheringInterface[TETHERING_TYPE_USB],
                           this.usbTetheringResultReport.bind(this));
    } else {
      this.usbTetheringResultReport("Failed to set usb function");
      throw new Error("failed to set USB Function to adb");
    }
  },

  usbTetheringResultReport: function(error) {
    let settingsLock = gSettingsService.createLock();

    this._usbTetheringAction = TETHERING_STATE_IDLE;
    // Disable tethering settings when fail to enable it.
    if (error) {
      this.tetheringSettings[SETTINGS_USB_ENABLED] = false;
      settingsLock.set("tethering.usb.enabled", false, null);
      // Skip others request when we found an error.
      this._requestCount = 0;
    } else {
      this.handleLastRequest();
    }
  },

  onConnectionChangedReport: function(success, externalIfname) {
    debug("onConnectionChangedReport result: success " + success);

    if (success) {
      // Update the external interface.
      this._tetheringInterface[TETHERING_TYPE_USB].externalInterface = externalIfname;
      debug("Change the interface name to " + externalIfname);
    }
  },

  onConnectionChanged: function(network) {
    if (network.state != Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED) {
      debug("We are only interested in CONNECTED event");
      return;
    }

    if (!this.tetheringSettings[SETTINGS_USB_ENABLED]) {
      debug("Usb tethering settings is not enabled");
      return;
    }

    if (this._tetheringInterface[TETHERING_TYPE_USB].externalInterface ===
        this.active.name) {
      debug("The active interface is the same");
      return;
    }

    let previous = {
      internalIfname: this._tetheringInterface[TETHERING_TYPE_USB].internalInterface,
      externalIfname: this._tetheringInterface[TETHERING_TYPE_USB].externalInterface
    };

    let current = {
      internalIfname: this._tetheringInterface[TETHERING_TYPE_USB].internalInterface,
      externalIfname: network.name
    };

    let callback = (function() {
      // Update external network interface.
      debug("Update upstream interface to " + network.name);
      gNetworkService.updateUpStream(previous, current, this.onConnectionChangedReport.bind(this));
    }).bind(this);

    if (this._usbTetheringAction === TETHERING_STATE_ONGOING) {
      debug("Postpone the event and handle it when state is idle.");
      this.wantConnectionEvent = callback;
      return;
    }
    this.wantConnectionEvent = null;

    callback.call(this);
  }
};

let CaptivePortalDetectionHelper = (function() {

  const EVENT_CONNECT = "Connect";
  const EVENT_DISCONNECT = "Disconnect";
  let _ongoingInterface = null;
  let _available = ("nsICaptivePortalDetector" in Ci);
  let getService = function() {
    return Cc['@mozilla.org/toolkit/captive-detector;1']
             .getService(Ci.nsICaptivePortalDetector);
  };

  let _performDetection = function(interfaceName, callback) {
    let capService = getService();
    let capCallback = {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsICaptivePortalCallback]),
      prepare: function() {
        capService.finishPreparation(interfaceName);
      },
      complete: function(success) {
        _ongoingInterface = null;
        callback(success);
      }
    };

    // Abort any unfinished captive portal detection.
    if (_ongoingInterface != null) {
      capService.abort(_ongoingInterface);
      _ongoingInterface = null;
    }
    try {
      capService.checkCaptivePortal(interfaceName, capCallback);
      _ongoingInterface = interfaceName;
    } catch (e) {
      debug('Fail to detect captive portal due to: ' + e.message);
    }
  };

  let _abort = function(interfaceName) {
    if (_ongoingInterface !== interfaceName) {
      return;
    }

    let capService = getService();
    capService.abort(_ongoingInterface);
    _ongoingInterface = null;
  };

  return {
    EVENT_CONNECT: EVENT_CONNECT,
    EVENT_DISCONNECT: EVENT_DISCONNECT,
    notify: function(eventType, network) {
      switch (eventType) {
        case EVENT_CONNECT:
          // perform captive portal detection on wifi interface
          if (_available && network &&
              network.type == Ci.nsINetworkInterface.NETWORK_TYPE_WIFI) {
            _performDetection(network.name, function() {
              // TODO: bug 837600
              // We can disconnect wifi in here if user abort the login procedure.
            });
          }

          break;
        case EVENT_DISCONNECT:
          if (_available &&
              network.type == Ci.nsINetworkInterface.NETWORK_TYPE_WIFI) {
            _abort(network.name);
          }
          break;
      }
    }
  };
}());

#ifdef MOZ_B2G_RIL
XPCOMUtils.defineLazyServiceGetter(NetworkManager.prototype, "mRil",
                                   "@mozilla.org/ril;1",
                                   "nsIRadioInterfaceLayer");
#endif

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NetworkManager]);


let debug;
if (DEBUG) {
  debug = function(s) {
    dump("-*- NetworkManager: " + s + "\n");
  };
} else {
  debug = function(s) {};
}
