/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const DEBUG = false;
function debug(s) { dump("-*- ResourceStatsManager: " + s + "\n"); }

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

// Constant defines supported statistics.
const resourceTypeList = ["network", "power"];

function NetworkStatsData(aStatsData) {
  if (DEBUG) {
    debug("NetworkStatsData(): " + JSON.stringify(aStatsData));
  }

  this.receivedBytes = aStatsData.receivedBytes || 0;
  this.sentBytes = aStatsData.sentBytes || 0;
  this.timestamp = aStatsData.timestamp;
}

NetworkStatsData.prototype = {
  classID: Components.ID("{dce5729a-ba92-4185-8854-e29e71b9e8a2}"),
  contractID: "@mozilla.org/networkStatsData;1",
  QueryInterface: XPCOMUtils.generateQI([])
};

function PowerStatsData(aStatsData) {
  if (DEBUG) {
    debug("PowerStatsData(): " + JSON.stringify(aStatsData));
  }

  this.consumedPower = aStatsData.consumedPower || 0;
  this.timestamp = aStatsData.timestamp;
}

PowerStatsData.prototype = {
  classID: Components.ID("{acb9af6c-8143-4e59-bc18-4bb1736a4004}"),
  contractID: "@mozilla.org/powerStatsData;1",
  QueryInterface: XPCOMUtils.generateQI([])
};

function ResourceStats(aWindow, aStats) {
  if (DEBUG) {
    debug("ResourceStats(): " + JSON.stringify(aStats));
  }

  this._window = aWindow;
  this.type = aStats.type;
  this.component = aStats.component || null;
  this.serviceType = aStats.serviceType || null;
  this.manifestURL = aStats.manifestURL || null;
  this.start = aStats.start;
  this.end = aStats.end;
  this.statsData = new aWindow.Array();

  // A function creates a StatsData object according to type.
  let createStatsDataObject = null;
  let self = this;
  switch (this.type) {
    case "power":
      createStatsDataObject = function(aStats) {
        let chromeObj = new PowerStatsData(aStats);
        return self._window.PowerStatsData._create(self._window, chromeObj);
      };
      break;
    case "network":
      createStatsDataObject = function(aStats) {
        let chromeObj = new NetworkStatsData(aStats);
        return self._window.NetworkStatsData._create(self._window, chromeObj);
      };
      break;
  }

  let sampleRate = aStats.sampleRate;
  let queryResult = aStats.statsData;
  let stats = queryResult.pop(); // Pop out the last element.
  let timestamp = this.start;

  // Push query result to statsData, and fill empty elements so that:
  // 1. the timestamp of the first element of statsData is equal to start;
  // 2. the timestamp of the last element of statsData is equal to end;
  // 3. the timestamp difference of every neighboring elements is SAMPLE_RATE.
  for (; timestamp <= this.end; timestamp += sampleRate) {
    if (!stats || stats.timestamp != timestamp) {
      // If dataArray is empty or timestamp are not equal, push a dummy object
      // (which stats are set to 0) to statsData.
      this.statsData.push(createStatsDataObject({ timestamp: timestamp }));
    } else {
      // Push stats to statsData and pop a new element form queryResult.
      this.statsData.push(createStatsDataObject(stats));
      stats = queryResult.pop();
    }
  }
}

ResourceStats.prototype = {
  getData: function() {
    return this.statsData;
  },

  classID: Components.ID("{b7c970f2-3d58-4966-9633-2024feb5132b}"),
  contractID: "@mozilla.org/resourceStats;1",
  QueryInterface: XPCOMUtils.generateQI([])
};

function ResourceStatsAlarm(aWindow, aAlarm) {
  if (DEBUG) {
    debug("ResourceStatsAlarm(): " + JSON.stringify(aAlarm));
  }

  this.alarmId = aAlarm.alarmId;
  this.type = aAlarm.type;
  this.component = aAlarm.component || null;
  this.serviceType = aAlarm.serviceType || null;
  this.manifestURL = aAlarm.manifestURL || null;
  this.threshold = aAlarm.threshold;

  // Clone data object using structured clone algorithm.
  this.data = null;
  if (aAlarm.data) {
    this.data = Cu.cloneInto(aAlarm.data, aWindow);
  }
}

ResourceStatsAlarm.prototype = {
  classID: Components.ID("{e2b66e7a-0ff1-4015-8690-a2a3f6a5b63a}"),
  contractID: "@mozilla.org/resourceStatsAlarm;1",
  QueryInterface: XPCOMUtils.generateQI([]),
};

function ResourceStatsManager() {
  if (DEBUG) {
    debug("constructor()");
  }
}

ResourceStatsManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  // Check time range.
  _checkTimeRange: function(aStart, aEnd) {
    if (DEBUG) {
      debug("_checkTimeRange(): " + aStart + " to " + aEnd);
    }

    let start = aStart ? aStart : 0;
    let end = aEnd ? aEnd : Date.now();

    if (start > end) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    return { start: start, end: end };
  },

  getStats: function(aStatsOptions, aStart, aEnd) {
    // Check time range.
    let { start: start, end: end } = this._checkTimeRange(aStart, aEnd);

    // Create Promise.
    let self = this;
    return this.createPromiseWithId(function(aResolverId) {
      self.cpmm.sendAsyncMessage("ResourceStats:GetStats", {
        resolverId: aResolverId,
        type: self.type,
        statsOptions: aStatsOptions,
        manifestURL: self._manifestURL,
        start: start,
        end: end
      });
    });
  },

  clearStats: function(aStatsOptions, aStart, aEnd) {
    // Check time range.
    let {start: start, end: end} = this._checkTimeRange(aStart, aEnd);

    // Create Promise.
    let self = this;
    return this.createPromiseWithId(function(aResolverId) {
      self.cpmm.sendAsyncMessage("ResourceStats:ClearStats", {
        resolverId: aResolverId,
        type: self.type,
        statsOptions: aStatsOptions,
        manifestURL: self._manifestURL,
        start: start,
        end: end
      });
    });
  },

  clearAllStats: function() {
    // Create Promise.
    let self = this;
    return this.createPromiseWithId(function(aResolverId) {
      self.cpmm.sendAsyncMessage("ResourceStats:ClearAllStats", {
        resolverId: aResolverId,
        type: self.type,
        manifestURL: self._manifestURL
      });
    });
  },

  addAlarm: function(aThreshold, aStatsOptions, aAlarmOptions) {
    if (DEBUG) {
      debug("aStatsOptions: " + JSON.stringify(aAlarmOptions));
      debug("aAlarmOptions: " + JSON.stringify(aAlarmOptions));
    }

    // Parse alarm options.
    let startTime = aAlarmOptions.startTime || 0;

    // Clone data object using structured clone algorithm.
    let data = null;
    if (aAlarmOptions.data) {
      data = Cu.cloneInto(aAlarmOptions.data, this._window);
    }

    // Create Promise.
    let self = this;
    return this.createPromiseWithId(function(aResolverId) {
      self.cpmm.sendAsyncMessage("ResourceStats:AddAlarm", {
        resolverId: aResolverId,
        type: self.type,
        threshold: aThreshold,
        statsOptions: aStatsOptions,
        manifestURL: self._manifestURL,
        startTime: startTime,
        data: data
      });
    });
  },

  getAlarms: function(aStatsOptions) {
    // Create Promise.
    let self = this;
    return this.createPromiseWithId(function(aResolverId) {
      self.cpmm.sendAsyncMessage("ResourceStats:GetAlarms", {
        resolverId: aResolverId,
        type: self.type,
        statsOptions: aStatsOptions,
        manifestURL: self._manifestURL
      });
    });
  },

  removeAlarm: function(aAlarmId) {
    // Create Promise.
    let self = this;
    return this.createPromiseWithId(function(aResolverId) {
      self.cpmm.sendAsyncMessage("ResourceStats:RemoveAlarm", {
        resolverId: aResolverId,
        type: self.type,
        manifestURL: self._manifestURL,
        alarmId: aAlarmId
      });
    });
  },

  removeAllAlarms: function() {
    // Create Promise.
    let self = this;
    return this.createPromiseWithId(function(aResolverId) {
      self.cpmm.sendAsyncMessage("ResourceStats:RemoveAllAlarms", {
        resolverId: aResolverId,
        type: self.type,
        manifestURL: self._manifestURL
      });
    });
  },

  getAvailableComponents: function() {
    // Create Promise.
    let self = this;
    return this.createPromiseWithId(function(aResolverId) {
      self.cpmm.sendAsyncMessage("ResourceStats:GetComponents", {
        resolverId: aResolverId,
        type: self.type,
        manifestURL: self._manifestURL
      });
    });
  },

  get resourceTypes() {
    let types = new this._window.Array();
    resourceTypeList.forEach(function(aType) {
      types.push(aType);
    });

    return types;
  },

  get sampleRate() {
    let msg = { manifestURL: this._manifestURL };

    return this.cpmm.sendSyncMessage("ResourceStats:SampleRate", msg)[0];
  },

  get maxStorageAge() {
    let msg = { manifestURL: this._manifestURL };

    return this.cpmm.sendSyncMessage("ResourceStats:MaxStorageAge", msg)[0];
  },

  receiveMessage: function(aMessage) {
    let data = aMessage.data;
    let chromeObj = null;
    let webidlObj = null;
    let self = this;

    if (DEBUG) {
      debug("receiveMessage(): " + aMessage.name + " " + data.resolverId);
    }

    let resolver = this.takePromiseResolver(data.resolverId);
    if (!resolver) {
      return;
    }

    switch (aMessage.name) {
      case "ResourceStats:GetStats:Resolved":
        if (DEBUG) {
          debug("data.value = " + JSON.stringify(data.value));
        }

        chromeObj = new ResourceStats(this._window, data.value);
        webidlObj = this._window.ResourceStats._create(this._window, chromeObj);
        resolver.resolve(webidlObj);
        break;

      case "ResourceStats:AddAlarm:Resolved":
        if (DEBUG) {
          debug("data.value = " + JSON.stringify(data.value));
        }

        resolver.resolve(data.value); // data.value is alarmId.
        break;

      case "ResourceStats:GetAlarms:Resolved":
        if (DEBUG) {
          debug("data.value = " + JSON.stringify(data.value));
        }

        let alarmArray = this._window.Array();
        data.value.forEach(function(aAlarm) {
          chromeObj = new ResourceStatsAlarm(self._window, aAlarm);
          webidlObj = self._window.ResourceStatsAlarm._create(self._window,
                                                              chromeObj);
          alarmArray.push(webidlObj);
        });
        resolver.resolve(alarmArray);
        break;

      case "ResourceStats:GetComponents:Resolved":
        if (DEBUG) {
          debug("data.value = " + JSON.stringify(data.value));
        }

        let components = this._window.Array();
        data.value.forEach(function(aComponent) {
          components.push(aComponent);
        });
        resolver.resolve(components);
        break;

      case "ResourceStats:ClearStats:Resolved":
      case "ResourceStats:ClearAllStats:Resolved":
      case "ResourceStats:RemoveAlarm:Resolved":
      case "ResourceStats:RemoveAllAlarms:Resolved":
        if (DEBUG) {
          debug("data.value = " + JSON.stringify(data.value));
        }

        resolver.resolve(data.value);
        break;

      case "ResourceStats:GetStats:Rejected":
      case "ResourceStats:ClearStats:Rejected":
      case "ResourceStats:ClearAllStats:Rejected":
      case "ResourceStats:AddAlarm:Rejected":
      case "ResourceStats:GetAlarms:Rejected":
      case "ResourceStats:RemoveAlarm:Rejected":
      case "ResourceStats:RemoveAllAlarms:Rejected":
      case "ResourceStats:GetComponents:Rejected":
        if (DEBUG) {
          debug("data.reason = " + JSON.stringify(data.reason));
        }

        resolver.reject(data.reason);
        break;

      default:
        if (DEBUG) {
          debug("Could not find a handler for " + aMessage.name);
        }
        resolver.reject();
    }
  },

  __init: function(aType) {
    if (resourceTypeList.indexOf(aType) < 0) {
      if (DEBUG) {
        debug("Do not support resource statistics for " + aType);
      }
      throw Cr.NS_ERROR_INVALID_ARG;
    } else {
      if (DEBUG) {
        debug("Create " + aType + "Mgr");
      }
      this.type = aType;
    }
  },

  init: function(aWindow) {
    if (DEBUG) {
      debug("init()");
    }

    // Get the manifestURL if this is an installed app
    let principal = aWindow.document.nodePrincipal;
    let appsService = Cc["@mozilla.org/AppsService;1"]
                        .getService(Ci.nsIAppsService);
    this._manifestURL = appsService.getManifestURLByLocalId(principal.appId);

    const messages = ["ResourceStats:GetStats:Resolved",
                      "ResourceStats:ClearStats:Resolved",
                      "ResourceStats:ClearAllStats:Resolved",
                      "ResourceStats:AddAlarm:Resolved",
                      "ResourceStats:GetAlarms:Resolved",
                      "ResourceStats:RemoveAlarm:Resolved",
                      "ResourceStats:RemoveAllAlarms:Resolved",
                      "ResourceStats:GetComponents:Resolved",
                      "ResourceStats:GetStats:Rejected",
                      "ResourceStats:ClearStats:Rejected",
                      "ResourceStats:ClearAllStats:Rejected",
                      "ResourceStats:AddAlarm:Rejected",
                      "ResourceStats:GetAlarms:Rejected",
                      "ResourceStats:RemoveAlarm:Rejected",
                      "ResourceStats:RemoveAllAlarms:Rejected",
                      "ResourceStats:GetComponents:Rejected"];
    this.initDOMRequestHelper(aWindow, messages);

    this.cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
                  .getService(Ci.nsISyncMessageSender);
  },

  classID: Components.ID("{101ed1f8-31b3-491c-95ea-04091e6e8027}"),
  contractID: "@mozilla.org/resourceStatsManager;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NetworkStatsData,
                                                     PowerStatsData,
                                                     ResourceStats,
                                                     ResourceStatsAlarm,
                                                     ResourceStatsManager]);

