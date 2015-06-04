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

const TOPIC_PREF_CHANGED             = "nsPref:changed";
const TOPIC_XPCOM_SHUTDOWN           = "xpcom-shutdown";
const PREF_NETWORK_DEBUG_ENABLED     = "network.debugging.enabled";

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

let debug;
function updateDebug() {
  let debugPref = false; // set default value here.
  try {
    debugPref = debugPref || Services.prefs.getBoolPref(PREF_NETWORK_DEBUG_ENABLED);
  } catch (e) {}

  if (debugPref) {
    debug = function(s) {
      dump("-*- NetworkService: " + s + "\n");
    };
  } else {
    debug = function(s) {};
  }
}
updateDebug();

function netdResponseType(aCode) {
  return Math.floor(aCode / 100) * 100;
}

function isError(aCode) {
  let type = netdResponseType(aCode);
  return (type !== NETD_COMMAND_PROCEEDING && type !== NETD_COMMAND_OKAY);
}

function Task(aId, aParams, aSetupFunction) {
  this.id = aId;
  this.params = aParams;
  this.setupFunction = aSetupFunction;
}

function NetworkWorkerRequestQueue(aNetworkService) {
  this.networkService = aNetworkService;
  this.tasks = [];
}
NetworkWorkerRequestQueue.prototype = {
  runQueue: function() {
    if (this.tasks.length === 0) {
      return;
    }

    let task = this.tasks[0];
    debug("run task id: " + task.id);

    if (typeof task.setupFunction === 'function') {
      // If setupFunction returns false, skip sending to Network Worker but call
      // handleWorkerMessage() directly with task id, as if the response was
      // returned from Network Worker.
      if (!task.setupFunction()) {
        this.networkService.handleWorkerMessage({id: task.id});
        return;
      }
    }

    gNetworkWorker.postMessage(task.params);
  },

  enqueue: function(aId, aParams, aSetupFunction) {
    debug("enqueue id: " + aId);
    this.tasks.push(new Task(aId, aParams, aSetupFunction));

    if (this.tasks.length === 1) {
      this.runQueue();
    }
  },

  dequeue: function(aId) {
    debug("dequeue id: " + aId);

    if (!this.tasks.length || this.tasks[0].id != aId) {
      debug("Id " + aId + " is not on top of the queue");
      return;
    }

    this.tasks.shift();
    if (this.tasks.length > 0) {
      // Run queue on the next tick.
      Services.tm.currentThread.dispatch(() => {
        this.runQueue();
      }, Ci.nsIThread.DISPATCH_NORMAL);
    }
  }
};


/**
 * This component watches for network interfaces changing state and then
 * adjusts routes etc. accordingly.
 */
function NetworkService() {
  debug("Starting NetworkService.");

  let self = this;

  if (gNetworkWorker) {
    let networkListener = {
      onEvent: function(aEvent) {
        self.handleWorkerMessage(aEvent);
      }
    };
    gNetworkWorker.start(networkListener);
  }
  // Callbacks to invoke when a reply arrives from the net_worker.
  this.controlCallbacks = Object.create(null);

  this.addedRoutes = new Map();
  this.netWorkerRequestQueue = new NetworkWorkerRequestQueue(this);
  this.shutdown = false;

  Services.prefs.addObserver(PREF_NETWORK_DEBUG_ENABLED, this, false);
  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN, false);
}

NetworkService.prototype = {
  classID:   NETWORKSERVICE_CID,
  classInfo: XPCOMUtils.generateCI({classID: NETWORKSERVICE_CID,
                                    contractID: NETWORKSERVICE_CONTRACTID,
                                    classDescription: "Network Service",
                                    interfaces: [Ci.nsINetworkService]}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkService,
                                         Ci.nsIObserver]),

  addedRoutes: null,

  shutdown: false,

  // nsIObserver

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case TOPIC_PREF_CHANGED:
        if (aData === PREF_NETWORK_DEBUG_ENABLED) {
          updateDebug();
        }
        break;
      case TOPIC_XPCOM_SHUTDOWN:
        debug("NetworkService shutdown");
        this.shutdown = true;
        if (gNetworkWorker) {
          gNetworkWorker.shutdown();
          gNetworkWorker = null;
        }

        Services.obs.removeObserver(this, TOPIC_XPCOM_SHUTDOWN);
        Services.prefs.removeObserver(PREF_NETWORK_DEBUG_ENABLED, this);
        break;
    }
  },

  // Helpers

  idgen: 0,
  controlMessage: function(aParams, aCallback, aSetupFunction) {
    if (this.shutdown) {
      return;
    }

    let id = this.idgen++;
    aParams.id = id;
    if (aCallback) {
      this.controlCallbacks[id] = aCallback;
    }

    // For now, we use aSetupFunction to determine if this command needs to be
    // queued or not.
    if (aSetupFunction) {
      this.netWorkerRequestQueue.enqueue(id, aParams, aSetupFunction);
      return;
    }

    if (gNetworkWorker) {
      gNetworkWorker.postMessage(aParams);
    }
  },

  handleWorkerMessage: function(aResponse) {
    debug("NetworkManager received message from worker: " + JSON.stringify(aResponse));
    let id = aResponse.id;
    if (aResponse.broadcast === true) {
      Services.obs.notifyObservers(null, aResponse.topic, aResponse.reason);
      return;
    }
    let callback = this.controlCallbacks[id];
    if (callback) {
      callback.call(this, aResponse);
      delete this.controlCallbacks[id];
    }

    this.netWorkerRequestQueue.dequeue(id);
  },

  // nsINetworkService

  getNetworkInterfaceStats: function(aInterfaceName, aCallback) {
    debug("getNetworkInterfaceStats for " + aInterfaceName);

    let file = new FileUtils.File("/proc/net/dev");
    if (!file) {
      aCallback.networkStatsAvailable(false, 0, 0, Date.now());
      return;
    }

    NetUtil.asyncFetch({
      uri: NetUtil.newURI(file),
      loadUsingSystemPrincipal: true
    }, function(inputStream, status) {
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
          if (parseResult && parseResult[1] === aInterfaceName) {
            rxBytes = parseInt(parseResult[2], 10);
            txBytes = parseInt(parseResult[3], 10);
            break;
          }
        }
      }

      // netd always return success even interface doesn't exist.
      aCallback.networkStatsAvailable(true, rxBytes, txBytes, now);
    });
  },

  setNetworkInterfaceAlarm: function(aInterfaceName, aThreshold, aCallback) {
    if (!aInterfaceName) {
      aCallback.networkUsageAlarmResult(-1);
      return;
    }

    let self = this;
    this._disableNetworkInterfaceAlarm(aInterfaceName, function(aResult) {
      if (aThreshold < 0) {
        if (!isError(aResult.resultCode)) {
          aCallback.networkUsageAlarmResult(null);
          return;
        }
        aCallback.networkUsageAlarmResult(aResult.reason);
        return
      }

      self._setNetworkInterfaceAlarm(aInterfaceName, aThreshold, aCallback);
    });
  },

  _setNetworkInterfaceAlarm: function(aInterfaceName, aThreshold, aCallback) {
    debug("setNetworkInterfaceAlarm for " + aInterfaceName + " at " + aThreshold + "bytes");

    let params = {
      cmd: "setNetworkInterfaceAlarm",
      ifname: aInterfaceName,
      threshold: aThreshold
    };

    params.report = true;

    this.controlMessage(params, function(aResult) {
      if (!isError(aResult.resultCode)) {
        aCallback.networkUsageAlarmResult(null);
        return;
      }

      this._enableNetworkInterfaceAlarm(aInterfaceName, aThreshold, aCallback);
    });
  },

  _enableNetworkInterfaceAlarm: function(aInterfaceName, aThreshold, aCallback) {
    debug("enableNetworkInterfaceAlarm for " + aInterfaceName + " at " + aThreshold + "bytes");

    let params = {
      cmd: "enableNetworkInterfaceAlarm",
      ifname: aInterfaceName,
      threshold: aThreshold
    };

    params.report = true;

    this.controlMessage(params, function(aResult) {
      if (!isError(aResult.resultCode)) {
        aCallback.networkUsageAlarmResult(null);
        return;
      }
      aCallback.networkUsageAlarmResult(aResult.reason);
    });
  },

  _disableNetworkInterfaceAlarm: function(aInterfaceName, aCallback) {
    debug("disableNetworkInterfaceAlarm for " + aInterfaceName);

    let params = {
      cmd: "disableNetworkInterfaceAlarm",
      ifname: aInterfaceName,
    };

    params.report = true;

    this.controlMessage(params, function(aResult) {
      aCallback(aResult);
    });
  },

  setWifiOperationMode: function(aInterfaceName, aMode, aCallback) {
    debug("setWifiOperationMode on " + aInterfaceName + " to " + aMode);

    let params = {
      cmd: "setWifiOperationMode",
      ifname: aInterfaceName,
      mode: aMode
    };

    params.report = true;

    this.controlMessage(params, function(aResult) {
      if (isError(aResult.resultCode)) {
        aCallback.wifiOperationModeResult("netd command error");
      } else {
        aCallback.wifiOperationModeResult(null);
      }
    });
  },

  resetRoutingTable: function(aInterfaceName, aCallback) {
    let options = {
      cmd: "removeNetworkRoute",
      ifname: aInterfaceName
    };

    this.controlMessage(options, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  setDNS: function(aInterfaceName, aDnsesCount, aDnses, aGatewaysCount,
                   aGateways, aCallback) {
    debug("Going to set DNS to " + aInterfaceName);
    let options = {
      cmd: "setDNS",
      ifname: aInterfaceName,
      domain: "mozilla." + aInterfaceName + ".domain",
      dnses: aDnses,
      gateways: aGateways
    };
    this.controlMessage(options, function(aResult) {
      aCallback.setDnsResult(aResult.success ? null : aResult.reason);
    });
  },

  setDefaultRoute: function(aInterfaceName, aCount, aGateways,
                            aOldInterfaceName, aCallback) {
    debug("Going to change default route to " + aInterfaceName);
    let options = {
      cmd: "setDefaultRoute",
      ifname: aInterfaceName,
      oldIfname: (aOldInterfaceName && aOldInterfaceName !== aInterfaceName) ?
                 aOldInterfaceName : null,
      gateways: aGateways
    };
    this.controlMessage(options, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  removeDefaultRoute: function(aInterfaceName, aCount, aGateways, aCallback) {
    debug("Remove default route for " + aInterfaceName);
    let options = {
      cmd: "removeDefaultRoute",
      ifname: aInterfaceName,
      gateways: aGateways
    };
    this.controlMessage(options, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  _routeToString: function(aInterfaceName, aHost, aPrefixLength, aGateway) {
    return aHost + "-" + aPrefixLength + "-" + aGateway + "-" + aInterfaceName;
  },

  modifyRoute: function(aAction, aInterfaceName, aHost, aPrefixLength, aGateway) {
    let command;

    switch (aAction) {
      case Ci.nsINetworkService.MODIFY_ROUTE_ADD:
        command = 'addHostRoute';
        break;
      case Ci.nsINetworkService.MODIFY_ROUTE_REMOVE:
        command = 'removeHostRoute';
        break;
      default:
        debug('Unknown action: ' + aAction);
        return Promise.reject();
    }

    let route = this._routeToString(aInterfaceName, aHost, aPrefixLength, aGateway);
    let setupFunc = () => {
      let count = this.addedRoutes.get(route);
      debug(command + ": " + route + " -> " + count);

      // Return false if there is no need to send the command to network worker.
      if ((aAction == Ci.nsINetworkService.MODIFY_ROUTE_ADD && count) ||
          (aAction == Ci.nsINetworkService.MODIFY_ROUTE_REMOVE &&
           (!count || count > 1))) {
        return false;
      }

      return true;
    };

    debug(command + " " + aHost + " on " + aInterfaceName);
    let options = {
      cmd: command,
      ifname: aInterfaceName,
      gateway: aGateway,
      prefixLength: aPrefixLength,
      ip: aHost
    };

    return new Promise((aResolve, aReject) => {
      this.controlMessage(options, (aData) => {
        let count = this.addedRoutes.get(route);

        // Remove route from addedRoutes on success or failure.
        if (aAction == Ci.nsINetworkService.MODIFY_ROUTE_REMOVE) {
          if (count > 1) {
            this.addedRoutes.set(route, count - 1);
          } else {
            this.addedRoutes.delete(route);
          }
        }

        if (aData.error) {
          aReject(aData.reason);
          return;
        }

        if (aAction == Ci.nsINetworkService.MODIFY_ROUTE_ADD) {
          this.addedRoutes.set(route, count ? count + 1 : 1);
        }

        aResolve();
      }, setupFunc);
    });
  },

  addSecondaryRoute: function(aInterfaceName, aRoute, aCallback) {
    debug("Going to add route to secondary table on " + aInterfaceName);
    let options = {
      cmd: "addSecondaryRoute",
      ifname: aInterfaceName,
      ip: aRoute.ip,
      prefix: aRoute.prefix,
      gateway: aRoute.gateway
    };
    this.controlMessage(options, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  removeSecondaryRoute: function(aInterfaceName, aRoute, aCallback) {
    debug("Going to remove route from secondary table on " + aInterfaceName);
    let options = {
      cmd: "removeSecondaryRoute",
      ifname: aInterfaceName,
      ip: aRoute.ip,
      prefix: aRoute.prefix,
      gateway: aRoute.gateway
    };
    this.controlMessage(options, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  // Enable/Disable DHCP server.
  setDhcpServer: function(aEnabled, aConfig, aCallback) {
    if (null === aConfig) {
      aConfig = {};
    }

    aConfig.cmd = "setDhcpServer";
    aConfig.enabled = aEnabled;

    this.controlMessage(aConfig, function(aResponse) {
      if (!aResponse.success) {
        aCallback.dhcpServerResult('Set DHCP server error');
        return;
      }
      aCallback.dhcpServerResult(null);
    });
  },

  // Enable/disable WiFi tethering by sending commands to netd.
  setWifiTethering: function(aEnable, aConfig, aCallback) {
    // config should've already contained:
    //   .ifname
    //   .internalIfname
    //   .externalIfname
    aConfig.wifictrlinterfacename = WIFI_CTRL_INTERFACE;
    aConfig.cmd = "setWifiTethering";

    // The callback function in controlMessage may not be fired immediately.
    this.controlMessage(aConfig, function(aData) {
      let code = aData.resultCode;
      let reason = aData.resultReason;
      let enable = aData.enable;
      let enableString = aEnable ? "Enable" : "Disable";

      debug(enableString + " Wifi tethering result: Code " + code + " reason " + reason);

      if (isError(code)) {
        aCallback.wifiTetheringEnabledChange("netd command error");
      } else {
        aCallback.wifiTetheringEnabledChange(null);
      }
    });
  },

  // Enable/disable USB tethering by sending commands to netd.
  setUSBTethering: function(aEnable, aConfig, aCallback) {
    aConfig.cmd = "setUSBTethering";
    // The callback function in controlMessage may not be fired immediately.
    this.controlMessage(aConfig, function(aData) {
      let code = aData.resultCode;
      let reason = aData.resultReason;
      let enable = aData.enable;
      let enableString = aEnable ? "Enable" : "Disable";

      debug(enableString + " USB tethering result: Code " + code + " reason " + reason);

      if (isError(code)) {
        aCallback.usbTetheringEnabledChange("netd command error");
      } else {
        aCallback.usbTetheringEnabledChange(null);
      }
    });
  },

  // Switch usb function by modifying property of persist.sys.usb.config.
  enableUsbRndis: function(aEnable, aCallback) {
    debug("enableUsbRndis: " + aEnable);

    let params = {
      cmd: "enableUsbRndis",
      enable: aEnable
    };
    // Ask net work to report the result when this value is set to true.
    if (aCallback) {
      params.report = true;
    } else {
      params.report = false;
    }

    // The callback function in controlMessage may not be fired immediately.
    //this._usbTetheringAction = TETHERING_STATE_ONGOING;
    this.controlMessage(params, function(aData) {
      aCallback.enableUsbRndisResult(aData.result, aData.enable);
    });
  },

  updateUpStream: function(aPrevious, aCurrent, aCallback) {
    let params = {
      cmd: "updateUpStream",
      preInternalIfname: aPrevious.internalIfname,
      preExternalIfname: aPrevious.externalIfname,
      curInternalIfname: aCurrent.internalIfname,
      curExternalIfname: aCurrent.externalIfname
    };

    this.controlMessage(params, function(aData) {
      let code = aData.resultCode;
      let reason = aData.resultReason;
      debug("updateUpStream result: Code " + code + " reason " + reason);
      aCallback.updateUpStreamResult(!isError(code), aData.curExternalIfname);
    });
  },

  configureInterface: function(aConfig, aCallback) {
    let params = {
      cmd: "configureInterface",
      ifname: aConfig.ifname,
      ipaddr: aConfig.ipaddr,
      mask: aConfig.mask,
      gateway_long: aConfig.gateway,
      dns1_long: aConfig.dns1,
      dns2_long: aConfig.dns2,
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  dhcpRequest: function(aInterfaceName, aCallback) {
    let params = {
      cmd: "dhcpRequest",
      ifname: aInterfaceName
    };

    this.controlMessage(params, function(aResult) {
      aCallback.dhcpRequestResult(!aResult.error, aResult.error ? null : aResult);
    });
  },

  stopDhcp: function(aInterfaceName, aCallback) {
    let params = {
      cmd: "stopDhcp",
      ifname: aInterfaceName
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  enableInterface: function(aInterfaceName, aCallback) {
    let params = {
      cmd: "enableInterface",
      ifname: aInterfaceName
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  disableInterface: function(aInterfaceName, aCallback) {
    let params = {
      cmd: "disableInterface",
      ifname: aInterfaceName
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  resetConnections: function(aInterfaceName, aCallback) {
    let params = {
      cmd: "resetConnections",
      ifname: aInterfaceName
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  createNetwork: function(aInterfaceName, aCallback) {
    let params = {
      cmd: "createNetwork",
      ifname: aInterfaceName
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  destroyNetwork: function(aInterfaceName, aCallback) {
    let params = {
      cmd: "destroyNetwork",
      ifname: aInterfaceName
    };

    this.controlMessage(params, function(aResult) {
      aCallback.nativeCommandResult(!aResult.error);
    });
  },

  getNetId: function(aInterfaceName) {
    let params = {
      cmd: "getNetId",
      ifname: aInterfaceName
    };

    return new Promise((aResolve, aReject) => {
      this.controlMessage(params, result => {
        if (result.error) {
          aReject(result.reason);
          return;
        }
        aResolve(result.netId);
      });
    });
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NetworkService]);
