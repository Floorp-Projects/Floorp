/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/systemlibs.js");
Cu.import("resource://gre/modules/Promise.jsm");

const NETWORKMANAGER_CONTRACTID = "@mozilla.org/network/manager;1";
const NETWORKMANAGER_CID =
  Components.ID("{33901e46-33b8-11e1-9869-f46d04d25bcc}");

const DEFAULT_PREFERRED_NETWORK_TYPE = Ci.nsINetworkInterface.NETWORK_TYPE_WIFI;

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

XPCOMUtils.defineLazyServiceGetter(this, "gPACGenerator",
                                   "@mozilla.org/pac-generator;1",
                                   "nsIPACGenerator");

const TOPIC_INTERFACE_REGISTERED     = "network-interface-registered";
const TOPIC_INTERFACE_UNREGISTERED   = "network-interface-unregistered";
const TOPIC_ACTIVE_CHANGED           = "network-active-changed";
const TOPIC_PREF_CHANGED             = "nsPref:changed";
const TOPIC_XPCOM_SHUTDOWN           = "xpcom-shutdown";
const TOPIC_CONNECTION_STATE_CHANGED = "network-connection-state-changed";
const PREF_MANAGE_OFFLINE_STATUS     = "network.gonk.manage-offline-status";
const PREF_NETWORK_DEBUG_ENABLED     = "network.debugging.enabled";

const IPV4_ADDRESS_ANY                 = "0.0.0.0";
const IPV6_ADDRESS_ANY                 = "::0";

const IPV4_MAX_PREFIX_LENGTH           = 32;
const IPV6_MAX_PREFIX_LENGTH           = 128;

// Connection Type for Network Information API
const CONNECTION_TYPE_CELLULAR  = 0;
const CONNECTION_TYPE_BLUETOOTH = 1;
const CONNECTION_TYPE_ETHERNET  = 2;
const CONNECTION_TYPE_WIFI      = 3;
const CONNECTION_TYPE_OTHER     = 4;
const CONNECTION_TYPE_NONE      = 5;

const PROXY_TYPE_MANUAL = Ci.nsIProtocolProxyService.PROXYCONFIG_MANUAL;
const PROXY_TYPE_PAC    = Ci.nsIProtocolProxyService.PROXYCONFIG_PAC;

let debug;
function updateDebug() {
  let debugPref = false; // set default value here.
  try {
    debugPref = debugPref || Services.prefs.getBoolPref(PREF_NETWORK_DEBUG_ENABLED);
  } catch (e) {}

  if (debugPref) {
    debug = function(s) {
      dump("-*- NetworkManager: " + s + "\n");
    };
  } else {
    debug = function(s) {};
  }
}
updateDebug();

function defineLazyRegExp(obj, name, pattern) {
  obj.__defineGetter__(name, function() {
    delete obj[name];
    return obj[name] = new RegExp(pattern);
  });
}

function NetworkInterface(aNetwork) {
  let ips = {};
  let prefixLengths = {};
  aNetwork.getAddresses(ips, prefixLengths);

  this.state = aNetwork.state;
  this.type = aNetwork.type;
  this.name = aNetwork.name;
  this.ips = ips.value;
  this.prefixLengths = prefixLengths.value;
  this.gateways = aNetwork.getGateways();
  this.dnses = aNetwork.getDnses();
  this.httpProxyHost = aNetwork.httpProxyHost;
  this.httpProxyPort = aNetwork.httpProxyPort;
}
NetworkInterface.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInterface]),

  getAddresses: function(aIps, aPrefixLengths) {
    aIps.value = this.ips.slice();
    aPrefixLengths.value = this.prefixLengths.slice();

    return this.ips.length;
  },

  getGateways: function(aCount) {
    if (aCount) {
      aCount.value = this.gateways.length;
    }
    return this.gateways.slice();
  },

  getDnses: function(aCount) {
    if (aCount) {
      aCount.value = this.dnses.length;
    }
    return this.dnses.slice();
  }
};

function NetworkInterfaceLinks()
{
  this.resetLinks();
}
NetworkInterfaceLinks.prototype = {
  linkRoutes: null,
  gateways: null,
  interfaceName: null,
  extraRoutes: null,

  setLinks: function(linkRoutes, gateways, interfaceName) {
    this.linkRoutes = linkRoutes;
    this.gateways = gateways;
    this.interfaceName = interfaceName;
  },

  resetLinks: function() {
    this.linkRoutes = [];
    this.gateways = [];
    this.interfaceName = "";
    this.extraRoutes = [];
  },

  compareGateways: function(gateways) {
    if (this.gateways.length != gateways.length) {
      return false;
    }

    for (let i = 0; i < this.gateways.length; i++) {
      if (this.gateways[i] != gateways[i]) {
        return false;
      }
    }

    return true;
  }
};

/**
 * This component watches for network interfaces changing state and then
 * adjusts routes etc. accordingly.
 */
function NetworkManager() {
  this.networkInterfaces = {};
  this.networkInterfaceLinks = {};

  try {
    this._manageOfflineStatus =
      Services.prefs.getBoolPref(PREF_MANAGE_OFFLINE_STATUS);
  } catch(ex) {
    // Ignore.
  }
  Services.prefs.addObserver(PREF_MANAGE_OFFLINE_STATUS, this, false);
  Services.prefs.addObserver(PREF_NETWORK_DEBUG_ENABLED, this, false);
  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN, false);

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
      case TOPIC_PREF_CHANGED:
        if (data === PREF_NETWORK_DEBUG_ENABLED) {
          updateDebug();
        } else if (data === PREF_MANAGE_OFFLINE_STATUS) {
          this._manageOfflineStatus =
            Services.prefs.getBoolPref(PREF_MANAGE_OFFLINE_STATUS);
          debug(PREF_MANAGE_OFFLINE_STATUS + " has changed to " + this._manageOfflineStatus);
        }
        break;
      case TOPIC_XPCOM_SHUTDOWN:
        Services.obs.removeObserver(this, TOPIC_XPCOM_SHUTDOWN);
        Services.prefs.removeObserver(PREF_MANAGE_OFFLINE_STATUS, this);
        Services.prefs.removeObserver(PREF_NETWORK_DEBUG_ENABLED, this);
        break;
    }
  },

  receiveMessage: function(aMsg) {
    switch (aMsg.name) {
      case "NetworkInterfaceList:ListInterface": {
        let excludeMms = aMsg.json.excludeMms;
        let excludeSupl = aMsg.json.excludeSupl;
        let excludeIms = aMsg.json.excludeIms;
        let excludeDun = aMsg.json.excludeDun;
        let interfaces = [];

        for each (let i in this.networkInterfaces) {
          if ((i.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS && excludeMms) ||
              (i.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL && excludeSupl) ||
              (i.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_IMS && excludeIms) ||
              (i.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_DUN && excludeDun)) {
            continue;
          }

          let ips = {};
          let prefixLengths = {};
          i.getAddresses(ips, prefixLengths);

          interfaces.push({
            state: i.state,
            type: i.type,
            name: i.name,
            ips: ips.value,
            prefixLengths: prefixLengths.value,
            gateways: i.getGateways(),
            dnses: i.getDnses(),
            httpProxyHost: i.httpProxyHost,
            httpProxyPort: i.httpProxyPort
          });
        }
        return interfaces;
      }
    }
  },

  getNetworkId: function(network) {
    let id = "device";
    try {
      if (network instanceof Ci.nsIRilNetworkInterface) {
        let rilNetwork = network.QueryInterface(Ci.nsIRilNetworkInterface);
        id = "ril" + rilNetwork.serviceId;
      }
    } catch (e) {}

    return id + "-" + network.type;
  },

  // nsINetworkManager

  registerNetworkInterface: function(network) {
    if (!(network instanceof Ci.nsINetworkInterface)) {
      throw Components.Exception("Argument must be nsINetworkInterface.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    let networkId = this.getNetworkId(network);
    if (networkId in this.networkInterfaces) {
      throw Components.Exception("Network with that type already registered!",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    this.networkInterfaces[networkId] = network;
    this.networkInterfaceLinks[networkId] = new NetworkInterfaceLinks();

    Services.obs.notifyObservers(network, TOPIC_INTERFACE_REGISTERED, null);
    debug("Network '" + networkId + "' registered.");
  },

  _addSubnetRoutes: function(network) {
    let ips = {};
    let prefixLengths = {};
    let length = network.getAddresses(ips, prefixLengths);
    let promises = [];

    for (let i = 0; i < length; i++) {
      debug('Adding subnet routes: ' + ips.value[i] + '/' + prefixLengths.value[i]);
      promises.push(
        gNetworkService.modifyRoute(Ci.nsINetworkService.MODIFY_ROUTE_ADD,
                                    network.name, ips.value[i], prefixLengths.value[i])
        .catch(aError => {
          debug("_addSubnetRoutes error: " + aError);
        }));
    }

    return Promise.all(promises);
  },

  updateNetworkInterface: function(network) {
    if (!(network instanceof Ci.nsINetworkInterface)) {
      throw Components.Exception("Argument must be nsINetworkInterface.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    let networkId = this.getNetworkId(network);
    if (!(networkId in this.networkInterfaces)) {
      throw Components.Exception("No network with that type registered.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    debug("Network " + network.type + "/" + network.name +
          " changed state to " + network.state);

    // Keep a copy of network in case it is modified while we are updating.
    let networkInterface = new NetworkInterface(network);

    // Note that since Lollipop we need to allocate and initialize
    // something through netd, so we add createNetwork/destroyNetwork
    // to deal with that explicitly.

    switch (networkInterface.state) {
      case Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED:

        this._createNetwork(networkInterface.name)
          // Remove pre-created default route and let setAndConfigureActive()
          // to set default route only on preferred network
          .then(() => this._removeDefaultRoute(networkInterface))
          // Set DNS server as early as possible to prevent from
          // premature domain name lookup.
          .then(() => this._setDNS(networkInterface))
          .then(() => {
            // Add host route for data calls
            if (!this.isNetworkTypeMobile(networkInterface.type)) {
              return;
            }

            let currentInterfaceLinks = this.networkInterfaceLinks[networkId];
            let newLinkRoutes = networkInterface.getDnses().concat(
              networkInterface.httpProxyHost);
            // If gateways have changed, remove all old routes first.
            return this._handleGateways(networkId, networkInterface.getGateways())
              .then(() => this._updateRoutes(currentInterfaceLinks.linkRoutes,
                                             newLinkRoutes,
                                             networkInterface.getGateways(),
                                             networkInterface.name))
              .then(() => currentInterfaceLinks.setLinks(newLinkRoutes,
                                                         networkInterface.getGateways(),
                                                         networkInterface.name));
          })
          .then(() => {
            if (networkInterface.type !=
                Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_DUN) {
              return;
            }
            // Dun type is a special case where we add the default route to a
            // secondary table.
            return this.setSecondaryDefaultRoute(networkInterface);
          })
          .then(() => this._addSubnetRoutes(networkInterface))
          .then(() => this.setAndConfigureActive())
          .then(() => {
            // Update data connection when Wifi connected/disconnected
            if (networkInterface.type ==
                Ci.nsINetworkInterface.NETWORK_TYPE_WIFI && this.mRil) {
              for (let i = 0; i < this.mRil.numRadioInterfaces; i++) {
                this.mRil.getRadioInterface(i).updateRILNetworkInterface();
              }
            }

            // Probing the public network accessibility after routing table is ready
            CaptivePortalDetectionHelper
              .notify(CaptivePortalDetectionHelper.EVENT_CONNECT, this.active);
          })
          .then(() => {
            // Notify outer modules like MmsService to start the transaction after
            // the configuration of the network interface is done.
            Services.obs.notifyObservers(network, TOPIC_CONNECTION_STATE_CHANGED,
                                         this.convertConnectionType(network));
          })
          .catch(aError => {
            debug("updateNetworkInterface error: " + aError);
          });
        break;
      case Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED:
        Promise.resolve()
          .then(() => {
            if (!this.isNetworkTypeMobile(networkInterface.type)) {
              return;
            }
            // Remove host route for data calls
            return this._cleanupAllHostRoutes(networkId);
          })
          .then(() => {
            if (networkInterface.type !=
                Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_DUN) {
              return;
            }
            // Remove secondary default route for dun.
            return this.removeSecondaryDefaultRoute(networkInterface);
          })
          .then(() => {
            if (networkInterface.type == Ci.nsINetworkInterface.NETWORK_TYPE_WIFI) {
              // Remove routing table in /proc/net/route
              return this._resetRoutingTable(networkInterface);
            }
            if (networkInterface.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE) {
              return this._removeDefaultRoute(networkInterface)
            }
          })
          .then(() => {
            // Clear http proxy on active network.
            if (this.active && networkInterface.type == this.active.type) {
              this.clearNetworkProxy();
            }

            // Abort ongoing captive portal detection on the wifi interface
            CaptivePortalDetectionHelper
              .notify(CaptivePortalDetectionHelper.EVENT_DISCONNECT, networkInterface);
          })
          .then(() => this.setAndConfigureActive())
          .then(() => {
            // Update data connection when Wifi connected/disconnected
            if (networkInterface.type ==
                Ci.nsINetworkInterface.NETWORK_TYPE_WIFI && this.mRil) {
              for (let i = 0; i < this.mRil.numRadioInterfaces; i++) {
                this.mRil.getRadioInterface(i).updateRILNetworkInterface();
              }
            }
          })
          .then(() => this._destroyNetwork(networkInterface.name))
          .then(() => {
            // Notify outer modules like MmsService to start the transaction after
            // the configuration of the network interface is done.
            Services.obs.notifyObservers(network, TOPIC_CONNECTION_STATE_CHANGED,
                                         this.convertConnectionType(network));
          })
          .catch(aError => {
            debug("updateNetworkInterface error: " + aError);
          });
        break;
    }
  },

  unregisterNetworkInterface: function(network) {
    if (!(network instanceof Ci.nsINetworkInterface)) {
      throw Components.Exception("Argument must be nsINetworkInterface.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    let networkId = this.getNetworkId(network);
    if (!(networkId in this.networkInterfaces)) {
      throw Components.Exception("No network with that type registered.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }

    // This is for in case a network gets unregistered without being
    // DISCONNECTED.
    if (this.isNetworkTypeMobile(network.type)) {
      this._cleanupAllHostRoutes(networkId);
    }

    delete this.networkInterfaces[networkId];

    Services.obs.notifyObservers(network, TOPIC_INTERFACE_UNREGISTERED, null);
    debug("Network '" + networkId + "' unregistered.");
  },

  _manageOfflineStatus: true,

  networkInterfaces: null,

  networkInterfaceLinks: null,

  _preferredNetworkType: DEFAULT_PREFERRED_NETWORK_TYPE,
  get preferredNetworkType() {
    return this._preferredNetworkType;
  },
  set preferredNetworkType(val) {
    if ([Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
         Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE].indexOf(val) == -1) {
      throw "Invalid network type";
    }
    this._preferredNetworkType = val;
  },

  active: null,
  _overriddenActive: null,

  overrideActive: function(network) {
    if ([Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
         Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE].indexOf(val) == -1) {
      throw "Invalid network type";
    }

    this._overriddenActive = network;
    this.setAndConfigureActive();
  },

  _updateRoutes: function(oldLinks, newLinks, gateways, interfaceName) {
    // Returns items that are in base but not in target.
    function getDifference(base, target) {
      return base.filter(function(i) { return target.indexOf(i) < 0; });
    }

    let addedLinks = getDifference(newLinks, oldLinks);
    let removedLinks = getDifference(oldLinks, newLinks);

    if (addedLinks.length === 0 && removedLinks.length === 0) {
      return Promise.resolve();
    }

    return this._setHostRoutes(false, removedLinks, interfaceName, gateways)
      .then(this._setHostRoutes(true, addedLinks, interfaceName, gateways));
  },

  _setHostRoutes: function(doAdd, ipAddresses, networkName, gateways) {
    let getMaxPrefixLength = (aIp) => {
      return aIp.match(this.REGEXP_IPV4) ? IPV4_MAX_PREFIX_LENGTH : IPV6_MAX_PREFIX_LENGTH;
    }

    let promises = [];

    ipAddresses.forEach((aIpAddress) => {
      let gateway = this.selectGateway(gateways, aIpAddress);
      if (gateway) {
        promises.push((doAdd)
          ? gNetworkService.modifyRoute(Ci.nsINetworkService.MODIFY_ROUTE_ADD,
                                        networkName, aIpAddress,
                                        getMaxPrefixLength(aIpAddress), gateway)
          : gNetworkService.modifyRoute(Ci.nsINetworkService.MODIFY_ROUTE_REMOVE,
                                        networkName, aIpAddress,
                                        getMaxPrefixLength(aIpAddress), gateway));
      }
    });

    return Promise.all(promises);
  },

  isValidatedNetwork: function(network) {
    let isValid = false;
    try {
      isValid = (this.getNetworkId(network) in this.networkInterfaces);
    } catch (e) {
      debug("Invalid network interface: " + e);
    }

    return isValid;
  },

  addHostRoute: function(network, host) {
    if (!this.isValidatedNetwork(network)) {
      return Promise.reject("Invalid network interface.");
    }

    return this.resolveHostname(network, host)
      .then((ipAddresses) => {
        let promises = [];
        let networkId = this.getNetworkId(network);

        ipAddresses.forEach((aIpAddress) => {
          let promise =
            this._setHostRoutes(true, [aIpAddress], network.name, network.getGateways())
              .then(() => this.networkInterfaceLinks[networkId].extraRoutes.push(aIpAddress));

          promises.push(promise);
        });

        return Promise.all(promises);
      });
  },

  removeHostRoute: function(network, host) {
    if (!this.isValidatedNetwork(network)) {
      return Promise.reject("Invalid network interface.");
    }

    return this.resolveHostname(network, host)
      .then((ipAddresses) => {
        let promises = [];
        let networkId = this.getNetworkId(network);

        ipAddresses.forEach((aIpAddress) => {
          let found = this.networkInterfaceLinks[networkId].extraRoutes.indexOf(aIpAddress);
          if (found < 0) {
            return; // continue
          }

          let promise =
            this._setHostRoutes(false, [aIpAddress], network.name, network.getGateways())
              .then(() => {
                this.networkInterfaceLinks[networkId].extraRoutes.splice(found, 1);
              }, () => {
                // We should remove it even if the operation failed.
                this.networkInterfaceLinks[networkId].extraRoutes.splice(found, 1);
              });
          promises.push(promise);
        });

        return Promise.all(promises);
      });
  },

  isNetworkTypeSecondaryMobile: function(type) {
    return (type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS ||
            type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL ||
            type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_IMS ||
            type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_DUN);
  },

  isNetworkTypeMobile: function(type) {
    return (type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE ||
            this.isNetworkTypeSecondaryMobile(type));
  },

  _handleGateways: function(networkId, gateways) {
    let currentNetworkLinks = this.networkInterfaceLinks[networkId];
    if (currentNetworkLinks.gateways.length == 0 ||
        currentNetworkLinks.compareGateways(gateways)) {
      return Promise.resolve();
    }

    let currentExtraRoutes = currentNetworkLinks.extraRoutes;
    return this._cleanupAllHostRoutes(networkId)
      .then(() => {
        // If gateways have changed, re-add extra host routes with new gateways.
        if (currentExtraRoutes.length > 0) {
          this._setHostRoutes(true,
                              currentExtraRoutes,
                              currentNetworkLinks.interfaceName,
                              gateways)
          .then(() => {
            currentNetworkLinks.extraRoutes = currentExtraRoutes;
          });
        }
      });
  },

  _cleanupAllHostRoutes: function(networkId) {
    let currentNetworkLinks = this.networkInterfaceLinks[networkId];
    let hostRoutes = currentNetworkLinks.linkRoutes.concat(
      currentNetworkLinks.extraRoutes);

    if (hostRoutes.length === 0) {
      return Promise.resolve();
    }

    return this._setHostRoutes(false,
                               hostRoutes,
                               currentNetworkLinks.interfaceName,
                               currentNetworkLinks.gateways)
      .catch((aError) => {
        debug("Error (" + aError + ") on _cleanupAllHostRoutes, keep proceeding.");
      })
      .then(() => currentNetworkLinks.resetLinks());
  },

  selectGateway: function(gateways, host) {
    for (let i = 0; i < gateways.length; i++) {
      let gateway = gateways[i];
      if (gateway.match(this.REGEXP_IPV4) && host.match(this.REGEXP_IPV4) ||
          gateway.indexOf(":") != -1 && host.indexOf(":") != -1) {
        return gateway;
      }
    }
    return null;
  },

  _setSecondaryRoute: function(aDoAdd, aInterfaceName, aRoute) {
    return new Promise((aResolve, aReject) => {
      if (aDoAdd) {
        gNetworkService.addSecondaryRoute(aInterfaceName, aRoute,
          (aSuccess) => {
            if (!aSuccess) {
              aReject("addSecondaryRoute failed");
              return;
            }
            aResolve();
        });
      } else {
        gNetworkService.removeSecondaryRoute(aInterfaceName, aRoute,
          (aSuccess) => {
            if (!aSuccess) {
              debug("removeSecondaryRoute failed")
            }
            // Always resolve.
            aResolve();
        });
      }
    });
  },

  setSecondaryDefaultRoute: function(network) {
    let gateways = network.getGateways();
    let promises = [];

    for (let i = 0; i < gateways.length; i++) {
      let isIPv6 = (gateways[i].indexOf(":") != -1) ? true : false;
      // First, we need to add a host route to the gateway in the secondary
      // routing table to make the gateway reachable. Host route takes the max
      // prefix and gateway address 'any'.
      let hostRoute = {
        ip: gateways[i],
        prefix: isIPv6 ? IPV6_MAX_PREFIX_LENGTH : IPV4_MAX_PREFIX_LENGTH,
        gateway: isIPv6 ? IPV6_ADDRESS_ANY : IPV4_ADDRESS_ANY
      };
      // Now we can add the default route through gateway. Default route takes the
      // min prefix and destination ip 'any'.
      let defaultRoute = {
        ip: isIPv6 ? IPV6_ADDRESS_ANY : IPV4_ADDRESS_ANY,
        prefix: 0,
        gateway: gateways[i]
      };

      let promise = this._setSecondaryRoute(true, network.name, hostRoute)
        .then(() => this._setSecondaryRoute(true, network.name, defaultRoute));

      promises.push(promise);
    }

    return Promise.all(promises);
  },

  removeSecondaryDefaultRoute: function(network) {
    let gateways = network.getGateways();
    let promises = [];

    for (let i = 0; i < gateways.length; i++) {
      let isIPv6 = (gateways[i].indexOf(":") != -1) ? true : false;
      // Remove both default route and host route.
      let defaultRoute = {
        ip: isIPv6 ? IPV6_ADDRESS_ANY : IPV4_ADDRESS_ANY,
        prefix: 0,
        gateway: gateways[i]
      };
      let hostRoute = {
        ip: gateways[i],
        prefix: isIPv6 ? IPV6_MAX_PREFIX_LENGTH : IPV4_MAX_PREFIX_LENGTH,
        gateway: isIPv6 ? IPV6_ADDRESS_ANY : IPV4_ADDRESS_ANY
      };

      let promise = this._setSecondaryRoute(false, network.name, defaultRoute)
        .then(() => this._setSecondaryRoute(false, network.name, hostRoute));

      promises.push(promise);
    }

    return Promise.all(promises);
  },

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
        this._setDefaultRouteAndProxy(this.active, oldActive);
        Services.obs.notifyObservers(this.active, TOPIC_ACTIVE_CHANGED, null);
      }
      return;
    }

    // The active network is already our preferred type.
    if (this.active &&
        this.active.state == Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED &&
        this.active.type == this._preferredNetworkType) {
      debug("Active network is already our preferred type.");
      return this._setDefaultRouteAndProxy(this.active, oldActive);
    }

    // Find a suitable network interface to activate.
    this.active = null;
    let anyConnected = false;

    for each (let network in this.networkInterfaces) {
      if (network.state != Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED) {
        continue;
      }
      anyConnected = true;

      // Set active only for default connections.
      if (network.type != Ci.nsINetworkInterface.NETWORK_TYPE_WIFI &&
          network.type != Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE) {
        continue;
      }

      this.active = network;
      if (network.type == this.preferredNetworkType) {
        debug("Found our preferred type of network: " + network.name);
        break;
      }
    }

    return Promise.resolve()
      .then(() => {
        if (!this.active) {
          return Promise.resolve();
        }

        return this._setDefaultRouteAndProxy(this.active, oldActive);
      })
      .then(() => {
        if (this.active != oldActive) {
          Services.obs.notifyObservers(this.active, TOPIC_ACTIVE_CHANGED, null);
        }

        if (this._manageOfflineStatus) {
          Services.io.offline = !anyConnected;
        }
      });
  },

  resolveHostname: function(network, hostname) {
    // Sanity check for null, undefined and empty string... etc.
    if (!hostname) {
      return Promise.reject(new Error("hostname is empty: " + hostname));
    }

    if (hostname.match(this.REGEXP_IPV4) ||
        hostname.match(this.REGEXP_IPV6)) {
      return Promise.resolve([hostname]);
    }

    // Wrap gDNSService.asyncResolveExtended to a promise, which
    // resolves with an array of ip addresses or rejects with
    // the reason otherwise.
    let hostResolveWrapper = aNetId => {
      return new Promise((aResolve, aReject) => {
        // Callback for gDNSService.asyncResolveExtended.
        let onLookupComplete = (aRequest, aRecord, aStatus) => {
          if (!Components.isSuccessCode(aStatus)) {
            aReject(new Error("Failed to resolve '" + hostname +
                              "', with status: " + aStatus));
            return;
          }

          let retval = [];
          while (aRecord.hasMore()) {
            retval.push(aRecord.getNextAddrAsString());
          }

          if (!retval.length) {
            aReject(new Error("No valid address after DNS lookup!"));
            return;
          }

          debug("hostname is resolved: " + hostname);
          debug("Addresses: " + JSON.stringify(retval));

          aResolve(retval);
        };

        debug('Calling gDNSService.asyncResolveExtended: ' + aNetId + ', ' + hostname);
        gDNSService.asyncResolveExtended(hostname,
                                         0,
                                         aNetId,
                                         onLookupComplete,
                                         Services.tm.mainThread);
      });
    };

    // TODO: |getNetId| will be implemented as a sync call in nsINetworkManager
    //       once Bug 1141903 is landed.
    return gNetworkService.getNetId(network.name)
      .then(aNetId => hostResolveWrapper(aNetId));
  },

  convertConnectionType: function(network) {
    // If there is internal interface change (e.g., MOBILE_MMS, MOBILE_SUPL),
    // the function will return null so that it won't trigger type change event
    // in NetworkInformation API.
    if (network.type != Ci.nsINetworkInterface.NETWORK_TYPE_WIFI &&
        network.type != Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE) {
      return null;
    }

    if (network.state == Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED) {
      return CONNECTION_TYPE_NONE;
    }

    switch (network.type) {
      case Ci.nsINetworkInterface.NETWORK_TYPE_WIFI:
        return CONNECTION_TYPE_WIFI;
      case Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE:
        return CONNECTION_TYPE_CELLULAR;
    }
  },

  _setDNS: function(aNetwork) {
    return new Promise((aResolve, aReject) => {
      gNetworkService.setDNS(aNetwork, (aError) => {
        if (aError) {
          aReject("setDNS failed");
          return;
        }
        aResolve();
      });
    });
  },

  _createNetwork: function(aInterfaceName) {
    return new Promise((aResolve, aReject) => {
      gNetworkService.createNetwork(aInterfaceName, (aSuccess) => {
        if (!aSuccess) {
          aReject("createNetwork failed");
          return;
        }
        aResolve();
      });
    });
  },

  _destroyNetwork: function(aInterfaceName) {
    return new Promise((aResolve, aReject) => {
      gNetworkService.destroyNetwork(aInterfaceName, (aSuccess) => {
        if (!aSuccess) {
          debug("destroyNetwork failed")
        }
        // Always resolve.
        aResolve();
      });
    });
  },

  _resetRoutingTable: function(aNetwork) {
    return new Promise((aResolve, aReject) => {
      gNetworkService.resetRoutingTable(aNetwork, (aSuccess) => {
        if (!aSuccess) {
          debug("resetRoutingTable failed");
        }
        // Always resolve.
        aResolve();
      });
    });
  },

  _removeDefaultRoute: function(aNetwork) {
    return new Promise((aResolve, aReject) => {
      gNetworkService.removeDefaultRoute(aNetwork, (aSuccess) => {
        if (!aSuccess) {
          debug("removeDefaultRoute failed");
        }
        // Always resolve.
        aResolve();
      });
    });
  },

  _setDefaultRouteAndProxy: function(aNetwork, aOldInterface) {
    return new Promise((aResolve, aReject) => {
      gNetworkService.setDefaultRoute(aNetwork, aOldInterface, (aSuccess) => {
        if (!aSuccess) {
          gNetworkService.destroyNetwork(aNetwork, function() {
            aReject("setDefaultRoute failed");
          });
          return;
        }
        this.setNetworkProxy(aNetwork);
        aResolve();
      });
    });
  },

  setNetworkProxy: function(network) {
    try {
      if (!network.httpProxyHost || network.httpProxyHost === "") {
        // Sets direct connection to internet.
        this.clearNetworkProxy();

        debug("No proxy support for " + network.name + " network interface.");
        return;
      }

      debug("Going to set proxy settings for " + network.name + " network interface.");

      // Do not use this proxy server for all protocols.
      Services.prefs.setBoolPref("network.proxy.share_proxy_settings", false);
      Services.prefs.setCharPref("network.proxy.http", network.httpProxyHost);
      Services.prefs.setCharPref("network.proxy.ssl", network.httpProxyHost);
      let port = network.httpProxyPort === 0 ? 8080 : network.httpProxyPort;
      Services.prefs.setIntPref("network.proxy.http_port", port);
      Services.prefs.setIntPref("network.proxy.ssl_port", port);

      let usePAC;
      try {
        usePAC = Services.prefs.getBoolPref("network.proxy.pac_generator");
      } catch (ex) {}

      if (usePAC) {
        Services.prefs.setCharPref("network.proxy.autoconfig_url",
                                   gPACGenerator.generate());
        Services.prefs.setIntPref("network.proxy.type", PROXY_TYPE_PAC);
      } else {
        Services.prefs.setIntPref("network.proxy.type", PROXY_TYPE_MANUAL);
      }
    } catch(ex) {
        debug("Exception " + ex + ". Unable to set proxy setting for " +
              network.name + " network interface.");
    }
  },

  clearNetworkProxy: function() {
    debug("Going to clear all network proxy.");

    Services.prefs.clearUserPref("network.proxy.share_proxy_settings");
    Services.prefs.clearUserPref("network.proxy.http");
    Services.prefs.clearUserPref("network.proxy.http_port");
    Services.prefs.clearUserPref("network.proxy.ssl");
    Services.prefs.clearUserPref("network.proxy.ssl_port");

    let usePAC;
    try {
      usePAC = Services.prefs.getBoolPref("network.proxy.pac_generator");
    } catch (ex) {}

    if (usePAC) {
      Services.prefs.setCharPref("network.proxy.autoconfig_url",
                                 gPACGenerator.generate());
      Services.prefs.setIntPref("network.proxy.type", PROXY_TYPE_PAC);
    } else {
      Services.prefs.clearUserPref("network.proxy.type");
    }
  },
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

XPCOMUtils.defineLazyGetter(NetworkManager.prototype, "mRil", function() {
  try {
    return Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
  } catch (e) {}

  return null;
});

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NetworkManager]);