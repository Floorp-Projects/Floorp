/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const TOPIC_INTERFACE_STATE_CHANGED = "network-interface-state-changed";

const ETHERNET_NETWORK_IFACE_PREFIX = "eth";
const DEFAULT_ETHERNET_NETWORK_IFACE = "eth0";

const INTERFACE_IPADDR_NULL = "0.0.0.0";
const INTERFACE_GATEWAY_NULL = "0.0.0.0";
const INTERFACE_PREFIX_NULL = 0;
const INTERFACE_MACADDR_NULL = "00:00:00:00:00:00";

const NETWORK_INTERFACE_UP   = "up";
const NETWORK_INTERFACE_DOWN = "down";

const IP_MODE_DHCP = "dhcp";
const IP_MODE_STATIC = "static";

const PREF_NETWORK_DEBUG_ENABLED = "network.debugging.enabled";

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkManager",
                                   "@mozilla.org/network/manager;1",
                                   "nsINetworkManager");

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkService",
                                   "@mozilla.org/network/service;1",
                                   "nsINetworkService");

let debug;
function updateDebug() {
  let debugPref = false; // set default value here.
  try {
    debugPref = debugPref || Services.prefs.getBoolPref(PREF_NETWORK_DEBUG_ENABLED);
  } catch (e) {}

  if (debugPref) {
    debug = function(s) {
      dump("-*- EthernetManager: " + s + "\n");
    };
  } else {
    debug = function(s) {};
  }
}
updateDebug();

// nsINetworkInterface

function EthernetInterface(attr) {
  this.info.state = attr.state;
  this.info.type = attr.type;
  this.info.name = attr.name;
  this.info.ipMode = attr.ipMode;
  this.info.ips = [attr.ip];
  this.info.prefixLengths = [attr.prefixLength];
  this.info.gateways = [attr.gateway];
  this.info.dnses = attr.dnses;
  this.httpProxyHost = "";
  this.httpProxyPort = 0;
}
EthernetInterface.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInterface]),

  updateConfig: function(config) {
    debug("Interface " + this.info.name + " updateConfig " + JSON.stringify(config));
    this.info.state = (config.state != undefined) ?
                  config.state : this.info.state;
    this.info.ips = (config.ip != undefined) ? [config.ip] : this.info.ips;
    this.info.prefixLengths = (config.prefixLength != undefined) ?
                         [config.prefixLength] : this.info.prefixLengths;
    this.info.gateways = (config.gateway != undefined) ?
                    [config.gateway] : this.info.gateways;
    this.info.dnses = (config.dnses != undefined) ? config.dnses : this.info.dnses;
    this.httpProxyHost = (config.httpProxyHost != undefined) ?
                          config.httpProxyHost : this.httpProxyHost;
    this.httpProxyPort = (config.httpProxyPort != undefined) ?
                          config.httpProxyPort : this.httpProxyPort;
    this.info.ipMode = (config.ipMode != undefined) ?
                   config.ipMode : this.info.ipMode;
  },

  info: {
    getAddresses: function(ips, prefixLengths) {
      ips.value = this.ips.slice();
      prefixLengths.value = this.prefixLengths.slice();

      return this.ips.length;
    },

    getGateways: function(count) {
      if (count) {
        count.value = this.gateways.length;
      }
      return this.gateways.slice();
    },

    getDnses: function(count) {
      if (count) {
        count.value = this.dnses.length;
      }
      return this.dnses.slice();
    }
  }
};

// nsIEthernetManager

/*
 *  Network state transition diagram
 *
 *   ----------  enable  ---------  connect   -----------   disconnect   --------------
 *  | Disabled | -----> | Enabled | -------> | Connected | <----------> | Disconnected |
 *   ----------          ---------            -----------    connect     --------------
 *       ^                  |                      |                           |
 *       |     disable      |                      |                           |
 *       -----------------------------------------------------------------------
 */

function EthernetManager() {
  debug("EthernetManager start");

  // Interface list.
  this.ethernetInterfaces = {};

  // Used to memorize last connection information.
  this.lastStaticConfig = {};

  Services.obs.addObserver(this, "xpcom-shutdown");
}

EthernetManager.prototype = {
  classID: Components.ID("a96441dd-36b3-4f7f-963b-2c032e28a039"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIEthernetManager]),

  ethernetInterfaces: null,
  lastStaticConfig: null,

  observer: function(subject, topic, data) {
    switch (topic) {
      case "xpcom-shutdown":
        debug("xpcom-shutdown");

        this._shutdown();

        Services.obs.removeObserver(this, "xpcom-shutdown");
        break;
    }
  },

  _shutdown: function() {
    debug("Shuting down");
    (function onRemove(ifnameList) {
      if (!ifnameList.length) {
        return;
      }

      let ifname = ifnameList.shift();
      this.removeInterface(ifname, { notify: onRemove.bind(this, ifnameList) });
    }).call(this, Object.keys(this.ethernetInterfaces));
  },

  get interfaceList() {
    return Object.keys(this.ethernetInterfaces);
  },

  scan: function(callback) {
    debug("Scan");

    gNetworkService.getInterfaces(function(success, list) {
      let ethList = [];

      if (!success) {
        if (callback) {
          callback.notify(ethList);
        }
        return;
      }

      for (let i = 0; i < list.length; i++) {
        debug("Found interface " + list[i]);
        if (!list[i].startsWith(ETHERNET_NETWORK_IFACE_PREFIX)) {
          continue;
        }
        ethList.push(list[i]);
      }

      if (callback) {
        callback.notify(ethList);
      }
    });
  },

  addInterface: function(ifname, callback) {
    debug("Add interface " + ifname);

    if (!ifname || !ifname.startsWith(ETHERNET_NETWORK_IFACE_PREFIX)) {
      if (callback) {
        callback.notify(false, "Invalid interface.");
      }
      return;
    }

    if (this.ethernetInterfaces[ifname]) {
      if (callback) {
        callback.notify(true, "Interface already exists.");
      }
      return;
    }

    gNetworkService.getInterfaceConfig(ifname, function(success, result) {
      if (!success) {
        if (callback) {
          callback.notify(false, "Netd error.");
        }
        return;
      }

      // Since the operation may still succeed with an invalid interface name,
      // check the mac address as well.
      if (result.macAddr == INTERFACE_MACADDR_NULL) {
        if (callback) {
          callback.notify(false, "Interface not found.");
        }
        return;
      }

      this.ethernetInterfaces[ifname] = new EthernetInterface({
        state:        result.link == NETWORK_INTERFACE_UP ?
                        Ci.nsINetworkInfo.NETWORK_STATE_DISABLED :
                        Ci.nsINetworkInfo.NETWORK_STATE_ENABLED,
        name:         ifname,
        type:         Ci.nsINetworkInfo.NETWORK_TYPE_ETHERNET,
        ip:           result.ip,
        prefixLength: result.prefix,
        ipMode:       IP_MODE_DHCP
      });

      // Register the interface to NetworkManager.
      gNetworkManager.registerNetworkInterface(this.ethernetInterfaces[ifname]);

      debug("Add interface " + ifname + " succeeded with " +
            JSON.stringify(this.ethernetInterfaces[ifname]));

      if (callback) {
        callback.notify(true, "ok");
      }
    }.bind(this));
  },

  removeInterface: function(ifname, callback) {
    debug("Remove interface " + ifname);

    if (!ifname || !ifname.startsWith(ETHERNET_NETWORK_IFACE_PREFIX)) {
      if (callback) {
        callback.notify(false, "Invalid interface.");
      }
      return;
    }

    if (!this.ethernetInterfaces[ifname]) {
      if (callback) {
        callback.notify(true, "Interface does not exist.");
      }
      return;
    }

    // Make sure interface is disable before removing.
    this.disable(ifname, { notify: function(success, message) {
      // Unregister the interface from NetworkManager and also remove it from
      // the interface list.
      gNetworkManager.unregisterNetworkInterface(this.ethernetInterfaces[ifname]);
      delete this.ethernetInterfaces[ifname];

      debug("Remove interface " + ifname + " succeeded.");

      if (callback) {
        callback.notify(true, "ok");
      }
    }.bind(this)});
  },

  updateInterfaceConfig: function(ifname, config, callback) {
    debug("Update interface config with " + ifname);

    this._ensureIfname(ifname, callback, function(iface) {
      if (!config) {
        if (callback) {
          callback.notify(false, "No config to update.");
        }
        return;
      }

      // Network state can not be modified externally.
      if (config.state) {
        delete config.state;
      }

      let currentIpMode = iface.info.ipMode;

      // Update config.
      this.ethernetInterfaces[iface.info.name].updateConfig(config);

      // Do not automatically re-connect if the interface is not in connected
      // state.
      if (iface.info.state != Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED) {
        if (callback) {
          callback.notify(true, "ok");
        }
        return;
      }

      let newIpMode = this.ethernetInterfaces[iface.info.name].info.ipMode;

      if (newIpMode == IP_MODE_STATIC) {
        this._setStaticIP(iface.info.name, callback);
        return;
      }
      if ((currentIpMode == IP_MODE_STATIC) && (newIpMode == IP_MODE_DHCP)) {
        gNetworkService.stopDhcp(iface.info.name, function(success) {
          if (success) {
            debug("DHCP for " + iface.info.name + " stopped.");
          }
        });

        // Clear the current network settings before do dhcp request, otherwise
        // dhcp settings could fail.
        this.disconnect(iface.info.name, { notify: function(success, message) {
          if (!success) {
            if (callback) {
              callback.notify("Disconnect failed.");
            }
            return;
          }
          this._runDhcp(iface.info.name, callback);
        }.bind(this) });
        return;
      }

      if (callback) {
        callback.notify(true, "ok");
      }
    }.bind(this));
  },

  enable: function(ifname, callback) {
    debug("Enable interface " + ifname);

    this._ensureIfname(ifname, callback, function(iface) {
      // Interface can be only enabled in the state of disabled.
      if (iface.info.state != Ci.nsINetworkInfo.NETWORK_STATE_DISABLED) {
        if (callback) {
          callback.notify(true, "Interface already enabled.");
        }
        return;
      }

      let ips = {};
      let prefixLengths = {};
      iface.info.getAddresses(ips, prefixLengths);
      let config = { ifname: iface.info.name,
                     ip:     ips.value[0],
                     prefix: prefixLengths.value[0],
                     link:   NETWORK_INTERFACE_UP };
      gNetworkService.setInterfaceConfig(config, function(success) {
        if (!success) {
          if (callback) {
            callback.notify(false, "Netd Error.");
          }
          return;
        }

        this.ethernetInterfaces[iface.info.name].updateConfig({
          state: Ci.nsINetworkInfo.NETWORK_STATE_ENABLED
        });

        debug("Enable interface " + iface.info.name + " succeeded.");

        if (callback) {
          callback.notify(true, "ok");
        }
      }.bind(this));
    }.bind(this));
  },

  disable: function(ifname, callback) {
    debug("Disable interface " + ifname);

    this._ensureIfname(ifname, callback, function(iface) {
      if (iface.info.state == Ci.nsINetworkInfo.NETWORK_STATE_DISABLED) {
        if (callback) {
          callback.notify(true, "Interface already disabled.");
        }
        return;
      }

      if (iface.info.state == Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED) {
        gNetworkService.stopDhcp(iface.info.name, function(success) {
          if (success) {
            debug("DHCP for " + iface.info.name + " stopped.");
          }
        });
      }

      let ips = {};
      let prefixLengths = {};
      iface.info.getAddresses(ips, prefixLengths);
      let config = { ifname: iface.info.name,
                     ip:     ips.value[0],
                     prefix: prefixLengths.value[0],
                     link:   NETWORK_INTERFACE_DOWN };
      gNetworkService.setInterfaceConfig(config, function(success) {
        if (!success) {
          if (callback) {
            callback.notify(false, "Netd Error.");
          }
          return;
        }

        this.ethernetInterfaces[iface.info.name].updateConfig({
          state: Ci.nsINetworkInfo.NETWORK_STATE_DISABLED
        });

        debug("Disable interface " + iface.info.name + " succeeded.");

        if (callback) {
          callback.notify(true, "ok");
        }
      }.bind(this));
    }.bind(this));
  },

  connect: function(ifname, callback) {
    debug("Connect interface " + ifname);

    this._ensureIfname(ifname, callback, function(iface) {
      // Interface can only be connected in the state of enabled or
      // disconnected.
      if (iface.info.state == Ci.nsINetworkInfo.NETWORK_STATE_DISABLED ||
          iface.info.state == Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED) {
        if (callback) {
          callback.notify(true, "Interface " + ifname + " is not available or "
                                 + " already connected.");
        }
        return;
      }

      if (iface.info.ipMode == IP_MODE_DHCP) {
        this._runDhcp(iface.info.name, callback);
        return;
      }

      if (iface.info.ipMode == IP_MODE_STATIC) {
        if (this._checkConfigNull(iface) && this.lastStaticConfig[iface.info.name]) {
          debug("Connect with lastStaticConfig " +
                JSON.stringify(this.lastStaticConfig[iface.info.name]));
          this.ethernetInterfaces[iface.info.name].updateConfig(
            this.lastStaticConfig[iface.info.name]);
        }
        this._setStaticIP(iface.info.name, callback);
        return;
      }

      if (callback) {
        callback.notify(false, "IP mode is wrong or not set.");
      }
    }.bind(this));
  },

  disconnect: function(ifname, callback) {
    debug("Disconnect interface " + ifname);

    this._ensureIfname(ifname, callback, function(iface) {
      // Interface can be only disconnected in the state of connected.
      if (iface.info.state != Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED) {
        if (callback) {
          callback.notify(true, "Interface is already disconnected");
        }
        return;
      }

      let config = { ifname: iface.info.name,
                     ip:     INTERFACE_IPADDR_NULL,
                     prefix: INTERFACE_PREFIX_NULL,
                     link:   NETWORK_INTERFACE_UP };
      gNetworkService.setInterfaceConfig(config, function(success) {
        if (!success) {
          if (callback) {
            callback.notify(false, "Netd error.");
          }
          return;
        }

        // Stop dhcp daemon.
        gNetworkService.stopDhcp(iface.info.name, function(success) {
          if (success) {
            debug("DHCP for " + iface.info.name + " stopped.");
          }
        });

        this.ethernetInterfaces[iface.info.name].updateConfig({
          state:        Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED,
          ip:           INTERFACE_IPADDR_NULL,
          prefixLength: INTERFACE_PREFIX_NULL,
          gateway:      INTERFACE_GATEWAY_NULL
        });

        gNetworkManager.updateNetworkInterface(this.ethernetInterfaces[ifname]);

        debug("Disconnect interface " + iface.info.name + " succeeded.");

        if (callback) {
          callback.notify(true, "ok");
        }
      }.bind(this));
    }.bind(this));
  },

  _checkConfigNull: function(iface) {
    let ips = {};
    let prefixLengths = {};
    let gateways = iface.info.getGateways();
    iface.info.getAddresses(ips, prefixLengths);

    if (ips.value[0] == INTERFACE_IPADDR_NULL &&
        prefixLengths.value[0] == INTERFACE_PREFIX_NULL &&
        gateways[0] == INTERFACE_GATEWAY_NULL) {
      return true;
    }

    return false;
  },

  _ensureIfname: function(ifname, callback, func) {
    // If no given ifname, use the default one.
    if (!ifname) {
      ifname = DEFAULT_ETHERNET_NETWORK_IFACE;
    }

    let iface = this.ethernetInterfaces[ifname];
    if (!iface) {
      if (callback) {
        callback.notify(true, "Interface " + ifname + " is not available.");
      }
      return;
    }

    func.call(this, iface);
  },

  _runDhcp: function(ifname, callback) {
    debug("runDhcp with " + ifname);

    if (!this.ethernetInterfaces[ifname]) {
      if (callback) {
        callback.notify(false, "Invalid interface.");
      }
      return;
    }

    gNetworkService.dhcpRequest(ifname, function(success, result) {
      if (!success) {
        if (callback) {
          callback.notify(false, "DHCP failed.");
        }
        return;
      }

      debug("DHCP succeeded with " + JSON.stringify(result));

      // Clear last static network information when connecting with dhcp mode.
      if (this.lastStaticConfig[ifname]) {
        this.lastStaticConfig[ifname] = null;
      }

      this.ethernetInterfaces[ifname].updateConfig({
        state:        Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED,
        ip:           result.ipaddr_str,
        gateway:      result.gateway_str,
        prefixLength: result.prefixLength,
        dnses:        [result.dns1_str, result.dns2_str]
      });

      gNetworkManager.updateNetworkInterface(this.ethernetInterfaces[ifname]);

      debug("Connect interface " + ifname + " with DHCP succeeded.");

      if (callback) {
        callback.notify(true, "ok");
      }
    }.bind(this));
  },

  _setStaticIP: function(ifname, callback) {
    let iface = this.ethernetInterfaces[ifname];
    if (!iface) {
      if (callback) {
        callback.notify(false, "Invalid interface.");
      }
      return;
    }

    let ips = {};
    let prefixLengths = {};
    iface.info.getAddresses(ips, prefixLengths);

    let config = { ifname: iface.info.name,
                   ip:     ips.value[0],
                   prefix: prefixLengths.value[0],
                   link:   NETWORK_INTERFACE_UP };
    gNetworkService.setInterfaceConfig(config, function(success) {
      if (!success) {
        if (callback) {
          callback.notify(false, "Netd Error.");
        }
        return;
      }

      // Keep the lastest static network information.
      let ips = {};
      let prefixLengths = {};
      let gateways = iface.info.getGateways();
      iface.info.getAddresses(ips, prefixLengths);

      this.lastStaticConfig[iface.info.name] = {
        ip:           ips.value[0],
        prefixLength: prefixLengths.value[0],
        gateway:      gateways[0]
      };

      this.ethernetInterfaces[ifname].updateConfig({
        state: Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED,
      });

      gNetworkManager.updateNetworkInterface(this.ethernetInterfaces[ifname]);

      debug("Connect interface " + ifname + " with static ip succeeded.");

      if (callback) {
        callback.notify(true, "ok");
      }
    }.bind(this));
  },
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([EthernetManager]);
