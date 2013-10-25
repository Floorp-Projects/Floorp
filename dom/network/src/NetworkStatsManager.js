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
Cu.import("resource://gre/modules/ObjectWrapper.jsm");

// Ensure NetworkStatsService and NetworkStatsDB are loaded in the parent process
// to receive messages from the child processes.
let appInfo = Cc["@mozilla.org/xre/app-info;1"];
let isParentProcess = !appInfo || appInfo.getService(Ci.nsIXULRuntime)
                        .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
if (isParentProcess) {
  Cu.import("resource://gre/modules/NetworkStatsService.jsm");
}

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

// NetworkStatsData
const nsIClassInfo              = Ci.nsIClassInfo;
const NETWORKSTATSDATA_CID      = Components.ID("{3b16fe17-5583-483a-b486-b64a3243221c}");
const nsIDOMMozNetworkStatsData = Components.interfaces.nsIDOMMozNetworkStatsData;

function NetworkStatsData(aData) {
  this.rxBytes = aData.rxBytes;
  this.txBytes = aData.txBytes;
  this.date = aData.date;
}

NetworkStatsData.prototype = {
  __exposedProps__: {
                      rxBytes: 'r',
                      txBytes: 'r',
                      date:  'r',
                     },

  classID : NETWORKSTATSDATA_CID,
  classInfo : XPCOMUtils.generateCI({classID: NETWORKSTATSDATA_CID,
                                     contractID:"@mozilla.org/networkstatsdata;1",
                                     classDescription: "NetworkStatsData",
                                     interfaces: [nsIDOMMozNetworkStatsData],
                                     flags: nsIClassInfo.DOM_OBJECT}),

  QueryInterface : XPCOMUtils.generateQI([nsIDOMMozNetworkStatsData])
};

// NetworkStatsInterface
const NETWORKSTATSINTERFACE_CONTRACTID = "@mozilla.org/networkstatsinterface;1";
const NETWORKSTATSINTERFACE_CID        = Components.ID("{f540615b-d803-43ff-8200-2a9d145a5645}");
const nsIDOMMozNetworkStatsInterface   = Components.interfaces.nsIDOMMozNetworkStatsInterface;

function NetworkStatsInterface(aNetwork) {
  if (DEBUG) {
    debug("NetworkStatsInterface Constructor");
  }
  this.type = aNetwork.type;
  this.id = aNetwork.id;
}

NetworkStatsInterface.prototype = {
  __exposedProps__: {
                      id: 'r',
                      type: 'r',
                    },

  classID : NETWORKSTATSINTERFACE_CID,
  classInfo : XPCOMUtils.generateCI({classID: NETWORKSTATSINTERFACE_CID,
                                     contractID: NETWORKSTATSINTERFACE_CONTRACTID,
                                     classDescription: "NetworkStatsInterface",
                                     interfaces: [nsIDOMMozNetworkStatsInterface],
                                     flags: nsIClassInfo.DOM_OBJECT}),

  QueryInterface : XPCOMUtils.generateQI([nsIDOMMozNetworkStatsInterface])
}

// NetworkStats
const NETWORKSTATS_CONTRACTID = "@mozilla.org/networkstats;1";
const NETWORKSTATS_CID        = Components.ID("{b6fc4b14-628d-4c99-bf4e-e4ed56916cbe}");
const nsIDOMMozNetworkStats   = Components.interfaces.nsIDOMMozNetworkStats;

function NetworkStats(aWindow, aStats) {
  if (DEBUG) {
    debug("NetworkStats Constructor");
  }
  this.manifestURL = aStats.manifestURL || null;
  this.network = new NetworkStatsInterface(aStats.network);
  this.start = aStats.start || null;
  this.end = aStats.end || null;

  let samples = this.data = Cu.createArrayIn(aWindow);
  for (let i = 0; i < aStats.data.length; i++) {
    samples.push(new NetworkStatsData(aStats.data[i]));
  }
}

NetworkStats.prototype = {
  __exposedProps__: {
                      manifestURL: 'r',
                      network: 'r',
                      start: 'r',
                      end:  'r',
                      data:  'r',
                    },

  classID : NETWORKSTATS_CID,
  classInfo : XPCOMUtils.generateCI({classID: NETWORKSTATS_CID,
                                     contractID: NETWORKSTATS_CONTRACTID,
                                     classDescription: "NetworkStats",
                                     interfaces: [nsIDOMMozNetworkStats],
                                     flags: nsIClassInfo.DOM_OBJECT}),

  QueryInterface : XPCOMUtils.generateQI([nsIDOMMozNetworkStats,
                                          nsIDOMMozNetworkStatsData,
                                          nsIDOMMozNetworkStatsInterface])
}

// NetworkStatsManager

const NETWORKSTATSMANAGER_CONTRACTID = "@mozilla.org/networkStatsManager;1";
const NETWORKSTATSMANAGER_CID        = Components.ID("{5fbdcae6-a2cd-47b3-929f-83ac75bd4881}");
const nsIDOMMozNetworkStatsManager   = Components.interfaces.nsIDOMMozNetworkStatsManager;

function NetworkStatsManager() {
  if (DEBUG) {
    debug("Constructor");
  }
}

NetworkStatsManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  checkPrivileges: function checkPrivileges() {
    if (!this.hasPrivileges) {
      throw Components.Exception("Permission denied", Cr.NS_ERROR_FAILURE);
    }
  },

  getSamples: function getSamples(aNetwork, aStart, aEnd, aManifestURL) {
    this.checkPrivileges();

    if (aStart.constructor.name !== "Date" ||
        aEnd.constructor.name !== "Date" ||
        aStart > aEnd) {
      throw Components.results.NS_ERROR_INVALID_ARG;
    }

    // TODO Bug 929410 Date object cannot correctly pass through cpmm/ppmm IPC
    // This is just a work-around by passing timestamp numbers.
    aStart = aStart.getTime();
    aEnd = aEnd.getTime();

    let request = this.createRequest();
    cpmm.sendAsyncMessage("NetworkStats:Get",
                          { network: aNetwork,
                            start: aStart,
                            end: aEnd,
                            manifestURL: aManifestURL,
                            id: this.getRequestId(request) });
    return request;
  },

  clearStats: function clearStats(aNetwork) {
    this.checkPrivileges();

    let request = this.createRequest();
    cpmm.sendAsyncMessage("NetworkStats:Clear",
                          { network: aNetwork,
                            id: this.getRequestId(request) });
    return request;
  },

  clearAllStats: function clearAllStats() {
    this.checkPrivileges();

    let request = this.createRequest();
    cpmm.sendAsyncMessage("NetworkStats:ClearAll",
                          {id: this.getRequestId(request)});
    return request;
  },

  get availableNetworks() {
    this.checkPrivileges();

    let result = ObjectWrapper.wrap(cpmm.sendSyncMessage("NetworkStats:Networks")[0], this._window);
    let networks = this.data = Cu.createArrayIn(this._window);
    for (let i = 0; i < result.length; i++) {
      networks.push(new NetworkStatsInterface(result[i]));
    }

    return networks;
  },

  get sampleRate() {
    this.checkPrivileges();
    return cpmm.sendSyncMessage("NetworkStats:SampleRate")[0];
  },

  get maxStorageAge() {
    this.checkPrivileges();
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

        let result = new NetworkStats(this._window, msg.result);
        if (DEBUG) {
          debug("result: " + JSON.stringify(result));
        }
        Services.DOMRequest.fireSuccess(req, result);
        break;

      case "NetworkStats:Clear:Return":
      case "NetworkStats:ClearAll:Return":
        if (msg.error) {
          Services.DOMRequest.fireError(req, msg.error);
          return;
        }

        Services.DOMRequest.fireSuccess(req, true);
        break;

      default:
        if (DEBUG) {
          debug("Wrong message: " + aMessage.name);
        }
    }
  },

  init: function(aWindow) {
    // Set navigator.mozNetworkStats to null.
    if (!Services.prefs.getBoolPref("dom.mozNetworkStats.enabled")) {
      return null;
    }

    let principal = aWindow.document.nodePrincipal;
    let secMan = Services.scriptSecurityManager;
    let perm = principal == secMan.getSystemPrincipal() ?
                 Ci.nsIPermissionManager.ALLOW_ACTION :
                 Services.perms.testExactPermissionFromPrincipal(principal,
                                                                 "networkstats-manage");

    // Only pages with perm set can use the netstats.
    this.hasPrivileges = perm == Ci.nsIPermissionManager.ALLOW_ACTION;
    if (DEBUG) {
      debug("has privileges: " + this.hasPrivileges);
    }

    if (!this.hasPrivileges) {
      return null;
    }

    this.initDOMRequestHelper(aWindow, ["NetworkStats:Get:Return",
                                        "NetworkStats:Clear:Return",
                                        "NetworkStats:ClearAll:Return"]);
  },

  // Called from DOMRequestIpcHelper
  uninit: function uninit() {
    if (DEBUG) {
      debug("uninit call");
    }
  },

  classID : NETWORKSTATSMANAGER_CID,
  QueryInterface : XPCOMUtils.generateQI([nsIDOMMozNetworkStatsManager,
                                         Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsISupportsWeakReference]),

  classInfo : XPCOMUtils.generateCI({classID: NETWORKSTATSMANAGER_CID,
                                     contractID: NETWORKSTATSMANAGER_CONTRACTID,
                                     classDescription: "NetworkStatsManager",
                                     interfaces: [nsIDOMMozNetworkStatsManager],
                                     flags: nsIClassInfo.DOM_OBJECT})
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NetworkStatsData,
                                                     NetworkStatsInterface,
                                                     NetworkStats,
                                                     NetworkStatsManager]);
