/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) { dump("-*- NetworkStatsManager: " + s + "\n"); }

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

// Ensure NetworkStatsService and NetworkStatsDB are loaded in the parent process
// to receive messages from the child processes.
var appInfo = Cc["@mozilla.org/xre/app-info;1"];
var isParentProcess = !appInfo || appInfo.getService(Ci.nsIXULRuntime)
                        .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
if (isParentProcess) {
  Cu.import("resource://gre/modules/NetworkStatsService.jsm");
}

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

// NetworkStatsData
const nsIClassInfo         = Ci.nsIClassInfo;
const NETWORKSTATSDATA_CID = Components.ID("{3b16fe17-5583-483a-b486-b64a3243221c}");

function NetworkStatsData(aWindow, aData) {
  this.rxBytes = aData.rxBytes;
  this.txBytes = aData.txBytes;
  this.date = new aWindow.Date(aData.date.getTime());
}

NetworkStatsData.prototype = {
  classID : NETWORKSTATSDATA_CID,

  QueryInterface : XPCOMUtils.generateQI([])
};

// NetworkStatsInterface
const NETWORKSTATSINTERFACE_CONTRACTID = "@mozilla.org/networkstatsinterface;1";
const NETWORKSTATSINTERFACE_CID = Components.ID("{f540615b-d803-43ff-8200-2a9d145a5645}");

function NetworkStatsInterface() {
  if (DEBUG) {
    debug("NetworkStatsInterface Constructor");
  }
}

NetworkStatsInterface.prototype = {
  __init: function(aNetwork) {
    this.type = aNetwork.type;
    this.id = aNetwork.id;
  },

  classID : NETWORKSTATSINTERFACE_CID,

  contractID: NETWORKSTATSINTERFACE_CONTRACTID,
  QueryInterface : XPCOMUtils.generateQI([])
}

// NetworkStats
const NETWORKSTATS_CID = Components.ID("{28904f59-8497-4ac0-904f-2af14b7fd3de}");

function NetworkStats(aWindow, aStats) {
  if (DEBUG) {
    debug("NetworkStats Constructor");
  }
  this.appManifestURL = aStats.appManifestURL || null;
  this.browsingTrafficOnly = aStats.browsingTrafficOnly || false;
  this.serviceType = aStats.serviceType || null;
  this.network = new aWindow.MozNetworkStatsInterface(aStats.network);
  this.start = aStats.start ? new aWindow.Date(aStats.start.getTime()) : null;
  this.end = aStats.end ? new aWindow.Date(aStats.end.getTime()) : null;

  let samples = this.data = new aWindow.Array();
  for (let i = 0; i < aStats.data.length; i++) {
    samples.push(aWindow.MozNetworkStatsData._create(
      aWindow, new NetworkStatsData(aWindow, aStats.data[i])));
  }
}

NetworkStats.prototype = {
  classID : NETWORKSTATS_CID,

  QueryInterface : XPCOMUtils.generateQI()
}

// NetworkStatsAlarm
const NETWORKSTATSALARM_CID = Components.ID("{a93ea13e-409c-4189-9b1e-95fff220be55}");

function NetworkStatsAlarm(aWindow, aAlarm) {
  this.alarmId = aAlarm.id;
  this.network = new aWindow.MozNetworkStatsInterface(aAlarm.network);
  this.threshold = aAlarm.threshold;
  this.data = aAlarm.data;
}

NetworkStatsAlarm.prototype = {
  classID : NETWORKSTATSALARM_CID,

  QueryInterface : XPCOMUtils.generateQI([])
};

// NetworkStatsManager

const NETWORKSTATSMANAGER_CONTRACTID = "@mozilla.org/networkStatsManager;1";
const NETWORKSTATSMANAGER_CID        = Components.ID("{ceb874cd-cc1a-4e65-b404-cc2d3e42425f}");

function NetworkStatsManager() {
  if (DEBUG) {
    debug("Constructor");
  }
}

NetworkStatsManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  getSamples: function getSamples(aNetwork, aStart, aEnd, aOptions) {
    if (aStart > aEnd) {
      throw Components.results.NS_ERROR_INVALID_ARG;
    }

    // appManifestURL is used to query network statistics by app;
    // serviceType is used to query network statistics by system service.
    // It is illegal to specify both of them at the same time.
    if (aOptions.appManifestURL && aOptions.serviceType) {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
    // browsingTrafficOnly is meaningful only when querying by app.
    if (!aOptions.appManifestURL && aOptions.browsingTrafficOnly) {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }

    let appManifestURL = aOptions.appManifestURL;
    let serviceType = aOptions.serviceType;
    let browsingTrafficOnly = aOptions.browsingTrafficOnly;

    // TODO Bug 929410 Date object cannot correctly pass through cpmm/ppmm IPC
    // This is just a work-around by passing timestamp numbers.
    aStart = aStart.getTime();
    aEnd = aEnd.getTime();

    let request = this.createRequest();
    cpmm.sendAsyncMessage("NetworkStats:Get",
                          { network: aNetwork.toJSON(),
                            start: aStart,
                            end: aEnd,
                            appManifestURL: appManifestURL,
                            browsingTrafficOnly: browsingTrafficOnly,
                            serviceType: serviceType,
                            id: this.getRequestId(request) });
    return request;
  },

  clearStats: function clearStats(aNetwork) {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("NetworkStats:Clear",
                          { network: aNetwork.toJSON(),
                            id: this.getRequestId(request) });
    return request;
  },

  clearAllStats: function clearAllStats() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("NetworkStats:ClearAll",
                          {id: this.getRequestId(request)});
    return request;
  },

  addAlarm: function addAlarm(aNetwork, aThreshold, aOptions) {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("NetworkStats:SetAlarm",
                          {id: this.getRequestId(request),
                           data: {network: aNetwork.toJSON(),
                                  threshold: aThreshold,
                                  startTime: aOptions.startTime,
                                  data: aOptions.data,
                                  manifestURL: "",
                                  pageURL: ""}});
    return request;
  },

  getAllAlarms: function getAllAlarms(aNetwork) {
    let network = null;
    if (aNetwork) {
      network = aNetwork.toJSON();
    }

    let request = this.createRequest();
    cpmm.sendAsyncMessage("NetworkStats:GetAlarms",
                          {id: this.getRequestId(request),
                           data: {network: network,
                                  manifestURL: ""}});
    return request;
  },

  removeAlarms: function removeAlarms(aAlarmId) {
    if (aAlarmId == 0) {
      aAlarmId = -1;
    }

    let request = this.createRequest();
    cpmm.sendAsyncMessage("NetworkStats:RemoveAlarms",
                          {id: this.getRequestId(request),
                           data: {alarmId: aAlarmId,
                                  manifestURL: ""}});

    return request;
  },

  getAvailableNetworks: function getAvailableNetworks() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("NetworkStats:GetAvailableNetworks",
                          { id: this.getRequestId(request) });
    return request;
  },

  getAvailableServiceTypes: function getAvailableServiceTypes() {
    let request = this.createRequest();
    cpmm.sendAsyncMessage("NetworkStats:GetAvailableServiceTypes",
                          { id: this.getRequestId(request) });
    return request;
  },

  get sampleRate() {
    return cpmm.sendSyncMessage("NetworkStats:SampleRate")[0];
  },

  get maxStorageAge() {
    return cpmm.sendSyncMessage("NetworkStats:MaxStorageAge")[0];
  },

  receiveMessage: function(aMessage) {
    if (DEBUG) {
      debug("NetworkStatsmanager::receiveMessage: " + aMessage.name);
    }

    let msg = aMessage.json;
    let req = this.takeRequest(msg.id);
    if (!req) {
      if (DEBUG) {
        debug("No request stored with id " + msg.id);
      }
      return;
    }

    switch (aMessage.name) {
      case "NetworkStats:Get:Return":
        if (msg.error) {
          Services.DOMRequest.fireError(req, msg.error);
          return;
        }

        let result = this._window.MozNetworkStats._create(
          this._window, new NetworkStats(this._window, msg.result));
        if (DEBUG) {
          debug("result: " + JSON.stringify(result));
        }
        Services.DOMRequest.fireSuccess(req, result);
        break;

      case "NetworkStats:GetAvailableNetworks:Return":
        if (msg.error) {
          Services.DOMRequest.fireError(req, msg.error);
          return;
        }

        let networks = new this._window.Array();
        for (let i = 0; i < msg.result.length; i++) {
          let network = new this._window.MozNetworkStatsInterface(msg.result[i]);
          networks.push(network);
        }

        Services.DOMRequest.fireSuccess(req, networks);
        break;

      case "NetworkStats:GetAvailableServiceTypes:Return":
        if (msg.error) {
          Services.DOMRequest.fireError(req, msg.error);
          return;
        }

        let serviceTypes = new this._window.Array();
        for (let i = 0; i < msg.result.length; i++) {
          serviceTypes.push(msg.result[i]);
        }

        Services.DOMRequest.fireSuccess(req, serviceTypes);
        break;

      case "NetworkStats:Clear:Return":
      case "NetworkStats:ClearAll:Return":
        if (msg.error) {
          Services.DOMRequest.fireError(req, msg.error);
          return;
        }

        Services.DOMRequest.fireSuccess(req, true);
        break;

      case "NetworkStats:SetAlarm:Return":
      case "NetworkStats:RemoveAlarms:Return":
        if (msg.error) {
          Services.DOMRequest.fireError(req, msg.error);
          return;
        }

        Services.DOMRequest.fireSuccess(req, msg.result);
        break;

      case "NetworkStats:GetAlarms:Return":
        if (msg.error) {
          Services.DOMRequest.fireError(req, msg.error);
          return;
        }

        let alarms = new this._window.Array();
        for (let i = 0; i < msg.result.length; i++) {
          // The WebIDL type of data is any, so we should manually clone it
          // into the content window.
          if ("data" in msg.result[i]) {
            msg.result[i].data = Cu.cloneInto(msg.result[i].data, this._window);
          }
          let alarm = new NetworkStatsAlarm(this._window, msg.result[i]);
          alarms.push(this._window.MozNetworkStatsAlarm._create(this._window, alarm));
        }

        Services.DOMRequest.fireSuccess(req, alarms);
        break;

      default:
        if (DEBUG) {
          debug("Wrong message: " + aMessage.name);
        }
    }
  },

  init: function(aWindow) {
    let principal = aWindow.document.nodePrincipal;

    this.initDOMRequestHelper(aWindow, ["NetworkStats:Get:Return",
                                        "NetworkStats:GetAvailableNetworks:Return",
                                        "NetworkStats:GetAvailableServiceTypes:Return",
                                        "NetworkStats:Clear:Return",
                                        "NetworkStats:ClearAll:Return",
                                        "NetworkStats:SetAlarm:Return",
                                        "NetworkStats:GetAlarms:Return",
                                        "NetworkStats:RemoveAlarms:Return"]);

    this.window = aWindow;
  },

  // Called from DOMRequestIpcHelper
  uninit: function uninit() {
    if (DEBUG) {
      debug("uninit call");
    }
  },

  classID : NETWORKSTATSMANAGER_CID,
  contractID : NETWORKSTATSMANAGER_CONTRACTID,
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                          Ci.nsISupportsWeakReference,
                                          Ci.nsIObserver]),
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NetworkStatsAlarm,
                                                     NetworkStatsData,
                                                     NetworkStatsInterface,
                                                     NetworkStats,
                                                     NetworkStatsManager]);
