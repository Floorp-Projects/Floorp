/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

const NETWORKSERVICE_CONTRACTID = "@mozilla.org/network/service;1";
const NETWORKSERVICE_CID = Components.ID("{baec696c-c78d-42db-8b44-603f8fbfafb4}");

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkWorker",
                                   "@mozilla.org/network/worker;1",
                                   "nsINetworkWorker");

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

const WIFI_CTRL_INTERFACE = "wl0.1";

const MANUAL_PROXY_CONFIGURATION = 1;

let DEBUG = false;

// Read debug setting from pref.
try {
  let debugPref = Services.prefs.getBoolPref("network.debugging.enabled");
  DEBUG = DEBUG || debugPref;
} catch (e) {}

function netdResponseType(code) {
  return Math.floor(code / 100) * 100;
}

function isError(code) {
  let type = netdResponseType(code);
  return (type !== NETD_COMMAND_PROCEEDING && type !== NETD_COMMAND_OKAY);
}

function debug(msg) {
  dump("-*- NetworkService: " + msg + "\n");
}

/**
 * This component watches for network interfaces changing state and then
 * adjusts routes etc. accordingly.
 */
function NetworkService() {
  if(DEBUG) debug("Starting net_worker.");

  let self = this;

  if (gNetworkWorker) {
    let networkListener = {
      onEvent: function(event) {
        self.handleWorkerMessage(event);
      }
    };
    gNetworkWorker.start(networkListener);
  }
  // Callbacks to invoke when a reply arrives from the net_worker.
  this.controlCallbacks = Object.create(null);

  this.shutdown = false;
  Services.obs.addObserver(this, "xpcom-shutdown", false);
}

NetworkService.prototype = {
  classID:   NETWORKSERVICE_CID,
  classInfo: XPCOMUtils.generateCI({classID: NETWORKSERVICE_CID,
                                    contractID: NETWORKSERVICE_CONTRACTID,
                                    classDescription: "Network Service",
                                    interfaces: [Ci.nsINetworkService]}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkService]),

  // Helpers

  idgen: 0,
  controlMessage: function(params, callback) {
    if (this.shutdown) {
      return;
    }

    if (callback) {
      let id = this.idgen++;
      params.id = id;
      this.controlCallbacks[id] = callback;
    }
    if (gNetworkWorker) {
      gNetworkWorker.postMessage(params);
    }
  },

  handleWorkerMessage: function(response) {
    if(DEBUG) debug("NetworkManager received message from worker: " + JSON.stringify(response));
    let id = response.id;
    if (response.broadcast === true) {
      Services.obs.notifyObservers(null, response.topic, response.reason);
      return;
    }
    let callback = this.controlCallbacks[id];
    if (callback) {
      callback.call(this, response);
      delete this.controlCallbacks[id];
    }
  },

  // nsINetworkService

  getNetworkInterfaceStats: function(networkName, callback) {
    if(DEBUG) debug("getNetworkInterfaceStats for " + networkName);

    let file = new FileUtils.File("/proc/net/dev");
    if (!file) {
      callback.networkStatsAvailable(false, 0, 0, Date.now());
      return;
    }

    NetUtil.asyncFetch(file, function(inputStream, status) {
      let rxBytes = 0,
          txBytes = 0,
          now = Date.now();

      if (Components.isSuccessCode(status)) {
        // Find record for corresponding interface.
        let statExpr = /(\S+): +(\d+) +\d+ +\d+ +\d+ +\d+ +\d+ +\d+ +\d+ +(\d+) +\d+ +\d+ +\d+ +\d+ +\d+ +\d+ +\d+/;
        let data =
          NetUtil.readInputStreamToString(inputStream, inputStream.available())
                 .split("\n");
        for (let i = 2; i < data.length; i++) {
          let parseResult = statExpr.exec(data[i]);
          if (parseResult && parseResult[1] === networkName) {
            rxBytes = parseInt(parseResult[2], 10);
            txBytes = parseInt(parseResult[3], 10);
            break;
          }
        }
      }

      // netd always return success even interface doesn't exist.
      callback.networkStatsAvailable(true, rxBytes, txBytes, now);
    });
  },

  setNetworkInterfaceAlarm: function(networkName, threshold, callback) {
    if (!networkName) {
      callback.networkUsageAlarmResult(-1);
      return;
    }

    let self = this;
    this._disableNetworkInterfaceAlarm(networkName, function(result) {
      if (threshold < 0) {
        if (!isError(result.resultCode)) {
          callback.networkUsageAlarmResult(null);
          return;
        }
        callback.networkUsageAlarmResult(result.reason);
        return
      }

      self._setNetworkInterfaceAlarm(networkName, threshold, callback);
    });
  },

  _setNetworkInterfaceAlarm: function(networkName, threshold, callback) {
    if(DEBUG) debug("setNetworkInterfaceAlarm for " + networkName + " at " + threshold + "bytes");

    let params = {
      cmd: "setNetworkInterfaceAlarm",
      ifname: networkName,
      threshold: threshold
    };

    params.report = true;
    params.isAsync = true;

    this.controlMessage(params, function(result) {
      if (!isError(result.resultCode)) {
        callback.networkUsageAlarmResult(null);
        return;
      }

      this._enableNetworkInterfaceAlarm(networkName, threshold, callback);
    });
  },

  _enableNetworkInterfaceAlarm: function(networkName, threshold, callback) {
    if(DEBUG) debug("enableNetworkInterfaceAlarm for " + networkName + " at " + threshold + "bytes");

    let params = {
      cmd: "enableNetworkInterfaceAlarm",
      ifname: networkName,
      threshold: threshold
    };

    params.report = true;
    params.isAsync = true;

    this.controlMessage(params, function(result) {
      if (!isError(result.resultCode)) {
        callback.networkUsageAlarmResult(null);
        return;
      }
      callback.networkUsageAlarmResult(result.reason);
    });
  },

  _disableNetworkInterfaceAlarm: function(networkName, callback) {
    if(DEBUG) debug("disableNetworkInterfaceAlarm for " + networkName);

    let params = {
      cmd: "disableNetworkInterfaceAlarm",
      ifname: networkName,
    };

    params.report = true;
    params.isAsync = true;

    this.controlMessage(params, function(result) {
      callback(result);
    });
  },

  setWifiOperationMode: function(interfaceName, mode, callback) {
    if(DEBUG) debug("setWifiOperationMode on " + interfaceName + " to " + mode);

    let params = {
      cmd: "setWifiOperationMode",
      ifname: interfaceName,
      mode: mode
    };

    params.report = true;
    params.isAsync = true;

    this.controlMessage(params, function(result) {
      if (isError(result.resultCode)) {
        callback.wifiOperationModeResult("netd command error");
      } else {
        callback.wifiOperationModeResult(null);
      }
    });
  },

  resetRoutingTable: function(network) {
    let ips = {};
    let prefixLengths = {};
    let length = network.getAddresses(ips, prefixLengths);

    for (let i = 0; i < length; i++) {
      let ip = ips.value[i];
      let prefixLength = prefixLengths.value[i];

      let options = {
        cmd: "removeNetworkRoute",
        ifname: network.name,
        ip: ip,
        prefixLength: prefixLength
      };
      this.controlMessage(options);
    }
  },

  setDNS: function(networkInterface) {
    if(DEBUG) debug("Going DNS to " + networkInterface.name);
    let dnses = networkInterface.getDnses();
    let options = {
      cmd: "setDNS",
      ifname: networkInterface.name,
      domain: "mozilla." + networkInterface.name + ".doman",
      dnses: dnses
    };
    this.controlMessage(options);
  },

  setDefaultRouteAndDNS: function(network, oldInterface) {
    if(DEBUG) debug("Going to change route and DNS to " + network.name);
    let gateways = network.getGateways();
    let dnses = network.getDnses();
    let options = {
      cmd: "setDefaultRouteAndDNS",
      ifname: network.name,
      oldIfname: (oldInterface && oldInterface !== network) ? oldInterface.name : null,
      gateways: gateways,
      domain: "mozilla." + network.name + ".doman",
      dnses: dnses
    };
    this.controlMessage(options);
    this.setNetworkProxy(network);
  },

  removeDefaultRoute: function(network) {
    if(DEBUG) debug("Remove default route for " + network.name);
    let gateways = network.getGateways();
    let options = {
      cmd: "removeDefaultRoute",
      ifname: network.name,
      gateways: gateways
    };
    this.controlMessage(options);
  },

  _setHostRoute: function(doAdd, interfaceName, gateway, host) {
    let command = doAdd ? "addHostRoute" : "removeHostRoute";

    if (DEBUG) debug(command + " " + host + " on " + interfaceName);
    let deferred = Promise.defer();
    let options = {
      cmd: command,
      ifname: interfaceName,
      gateway: gateway,
      ip: host
    };
    this.controlMessage(options, function(data) {
      if (data.error) {
        deferred.reject(data.reason);
        return;
      }
      deferred.resolve();
    });
    return deferred.promise;
  },

  addHostRoute: function(interfaceName, gateway, host) {
    return this._setHostRoute(true, interfaceName, gateway, host);
  },

  removeHostRoute: function(interfaceName, gateway, host) {
    return this._setHostRoute(false, interfaceName, gateway, host);
  },

  removeHostRoutes: function(ifname) {
    if(DEBUG) debug("Going to remove all host routes on " + ifname);
    let options = {
      cmd: "removeHostRoutes",
      ifname: ifname,
    };
    this.controlMessage(options);
  },

  addSecondaryRoute: function(ifname, route) {
    if(DEBUG) debug("Going to add route to secondary table on " + ifname);
    let options = {
      cmd: "addSecondaryRoute",
      ifname: ifname,
      ip: route.ip,
      prefix: route.prefix,
      gateway: route.gateway
    };
    this.controlMessage(options);
  },

  removeSecondaryRoute: function(ifname, route) {
    if(DEBUG) debug("Going to remove route from secondary table on " + ifname);
    let options = {
      cmd: "removeSecondaryRoute",
      ifname: ifname,
      ip: route.ip,
      prefix: route.prefix,
      gateway: route.gateway
    };
    this.controlMessage(options);
  },

  setNetworkProxy: function(network) {
    try {
      if (!network.httpProxyHost || network.httpProxyHost === "") {
        // Sets direct connection to internet.
        Services.prefs.clearUserPref("network.proxy.type");
        Services.prefs.clearUserPref("network.proxy.share_proxy_settings");
        Services.prefs.clearUserPref("network.proxy.http");
        Services.prefs.clearUserPref("network.proxy.http_port");
        Services.prefs.clearUserPref("network.proxy.ssl");
        Services.prefs.clearUserPref("network.proxy.ssl_port");
        if(DEBUG) debug("No proxy support for " + network.name + " network interface.");
        return;
      }

      if(DEBUG) debug("Going to set proxy settings for " + network.name + " network interface.");
      // Sets manual proxy configuration.
      Services.prefs.setIntPref("network.proxy.type", MANUAL_PROXY_CONFIGURATION);
      // Do not use this proxy server for all protocols.
      Services.prefs.setBoolPref("network.proxy.share_proxy_settings", false);
      Services.prefs.setCharPref("network.proxy.http", network.httpProxyHost);
      Services.prefs.setCharPref("network.proxy.ssl", network.httpProxyHost);
      let port = network.httpProxyPort === 0 ? 8080 : network.httpProxyPort;
      Services.prefs.setIntPref("network.proxy.http_port", port);
      Services.prefs.setIntPref("network.proxy.ssl_port", port);
    } catch(ex) {
        if(DEBUG) debug("Exception " + ex + ". Unable to set proxy setting for " +
                         network.name + " network interface.");
    }
  },

  // Enable/Disable DHCP server.
  setDhcpServer: function(enabled, config, callback) {
    if (null === config) {
      config = {};
    }

    config.cmd = "setDhcpServer";
    config.isAsync = true;
    config.enabled = enabled;

    this.controlMessage(config, function setDhcpServerResult(response) {
      if (!response.success) {
        callback.dhcpServerResult('Set DHCP server error');
        return;
      }
      callback.dhcpServerResult(null);
    });
  },

  // Enable/disable WiFi tethering by sending commands to netd.
  setWifiTethering: function(enable, config, callback) {
    // config should've already contained:
    //   .ifname
    //   .internalIfname
    //   .externalIfname
    config.wifictrlinterfacename = WIFI_CTRL_INTERFACE;
    config.cmd = "setWifiTethering";

    // The callback function in controlMessage may not be fired immediately.
    config.isAsync = true;
    this.controlMessage(config, function setWifiTetheringResult(data) {
      let code = data.resultCode;
      let reason = data.resultReason;
      let enable = data.enable;
      let enableString = enable ? "Enable" : "Disable";

      if(DEBUG) debug(enableString + " Wifi tethering result: Code " + code + " reason " + reason);

      if (isError(code)) {
        callback.wifiTetheringEnabledChange("netd command error");
      } else {
        callback.wifiTetheringEnabledChange(null);
      }
    });
  },

  // Enable/disable USB tethering by sending commands to netd.
  setUSBTethering: function(enable, config, callback) {
    config.cmd = "setUSBTethering";
    // The callback function in controlMessage may not be fired immediately.
    config.isAsync = true;
    this.controlMessage(config, function setUsbTetheringResult(data) {
      let code = data.resultCode;
      let reason = data.resultReason;
      let enable = data.enable;
      let enableString = enable ? "Enable" : "Disable";

      if(DEBUG) debug(enableString + " USB tethering result: Code " + code + " reason " + reason);

      if (isError(code)) {
        callback.usbTetheringEnabledChange("netd command error");
      } else {
        callback.usbTetheringEnabledChange(null);
      }
    });
  },

  // Switch usb function by modifying property of persist.sys.usb.config.
  enableUsbRndis: function(enable, callback) {
    if(DEBUG) debug("enableUsbRndis: " + enable);

    let params = {
      cmd: "enableUsbRndis",
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
    //this._usbTetheringAction = TETHERING_STATE_ONGOING;
    this.controlMessage(params, function(data) {
      callback.enableUsbRndisResult(data.result, data.enable);
    });
  },

  updateUpStream: function(previous, current, callback) {
    let params = {
      cmd: "updateUpStream",
      isAsync: true,
      preInternalIfname: previous.internalIfname,
      preExternalIfname: previous.externalIfname,
      curInternalIfname: current.internalIfname,
      curExternalIfname: current.externalIfname
    };

    this.controlMessage(params, function(data) {
      let code = data.resultCode;
      let reason = data.resultReason;
      if(DEBUG) debug("updateUpStream result: Code " + code + " reason " + reason);
      callback.updateUpStreamResult(!isError(code), data.curExternalIfname);
    });
  },

  shutdown: false,

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "xpcom-shutdown":
        debug("NetworkService shutdown");
        this.shutdown = true;
        Services.obs.removeObserver(this, "xpcom-shutdown");
        if (gNetworkWorker) {
          gNetworkWorker.shutdown();
          gNetworkWorker = null;
        }
        break;
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NetworkService]);
