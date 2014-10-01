/* This Source Code Form is subject to the terms of the Mozilla public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

this.EXPORTED_SYMBOLS = ['ResourceStatsDB'];

const DEBUG = false;
function debug(s) { dump("-*- ResourceStatsDB: " + s + "\n"); }

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/IndexedDBHelper.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.importGlobalProperties(["indexedDB"]);

XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

const DB_NAME = "resource_stats";
const DB_VERSION = 1;
const POWER_STATS_STORE = "power_stats_store";
const NETWORK_STATS_STORE = "network_stats_store";
const ALARM_STORE = "alarm_store";

const statsStoreNames = {
  power: POWER_STATS_STORE,
  network: NETWORK_STATS_STORE
};

// Constant defining the sampling rate.
const SAMPLE_RATE = 24 * 60 * 60 * 1000; // 1 day.

// Constant defining the MAX age of stored stats.
const MAX_STORAGE_AGE = 180 * SAMPLE_RATE; // 180 days.

this.ResourceStatsDB = function ResourceStatsDB() {
  if (DEBUG) {
    debug("Constructor()");
  }

  this.initDBHelper(DB_NAME, DB_VERSION,
                    [POWER_STATS_STORE, NETWORK_STATS_STORE, ALARM_STORE]);
};

ResourceStatsDB.prototype = {
  __proto__: IndexedDBHelper.prototype,

  _dbNewTxn: function(aStoreName, aTxnType, aCallback, aTxnCb) {
    function successCb(aResult) {
      aTxnCb(null, aResult);
    }
    function errorCb(aError) {
      aTxnCb(aError, null);
    }
    return this.newTxn(aTxnType, aStoreName, aCallback, successCb, errorCb);
  },

  upgradeSchema: function(aTransaction, aDb, aOldVersion, aNewVersion) {
    if (DEBUG) {
      debug("Upgrade DB from ver." + aOldVersion + " to ver." + aNewVersion);
    }

    let objectStore;

    // Create PowerStatsStore.
    objectStore = aDb.createObjectStore(POWER_STATS_STORE, {
      keyPath: ["appId", "serviceType", "component", "timestamp"]
    });
    objectStore.createIndex("component", "component", { unique: false });

    // Create NetworkStatsStore.
    objectStore = aDb.createObjectStore(NETWORK_STATS_STORE, {
      keyPath: ["appId", "serviceType", "component", "timestamp"]
    });
    objectStore.createIndex("component", "component", { unique: false });

    // Create AlarmStore.
    objectStore = aDb.createObjectStore(ALARM_STORE, {
      keyPath: "alarmId",
      autoIncrement: true
    });
    objectStore.createIndex("type", "type", { unique: false });
    // Index for resource control target.
    objectStore.createIndex("controlTarget",
                            ["type", "manifestURL", "serviceType", "component"],
                            { unique: false });
  },

  // Convert to UTC according to the current timezone and the filter timestamp
  // to get SAMPLE_RATE precission.
  _normalizeTime: function(aTime, aOffset) {
    let time = Math.floor((aTime - aOffset) / SAMPLE_RATE) * SAMPLE_RATE;

    return time;
  },

  /**
   * aRecordArray contains an array of json objects storing network stats.
   * The structure of the json object =
   *  {
   *    appId: XX,
   *    serviceType: "XX",
   *    componentStats: {
   *      "component_1": { receivedBytes: XX, sentBytes: XX },
   *      "component_2": { receivedBytes: XX, sentBytes: XX },
   *      ...
   *    },
   *  }
   */
  saveNetworkStats: function(aRecordArray, aTimestamp, aResultCb) {
    if (DEBUG) {
      debug("saveNetworkStats()");
    }

    let offset = (new Date()).getTimezoneOffset() * 60 * 1000;
    let timestamp = this._normalizeTime(aTimestamp, offset);

    this._dbNewTxn(NETWORK_STATS_STORE, "readwrite", function(aTxn, aStore) {
      aRecordArray.forEach(function(aRecord) {
        let stats = {
          appId: aRecord.appId,
          serviceType: aRecord.serviceType,
          component: "",
          timestamp: timestamp,
          receivedBytes: 0,
          sentBytes: 0
        };

        let totalReceivedBytes = 0;
        let totalSentBytes = 0;

        // Save stats of each component.
        let data = aRecord.componentStats;
        for (let component in data) {
          // Save stats to database.
          stats.component = component;
          stats.receivedBytes = data[component].receivedBytes;
          stats.sentBytes = data[component].sentBytes;
          aStore.put(stats);
          if (DEBUG) {
            debug("Save network stats: " + JSON.stringify(stats));
          }

          // Accumulated to tatal stats.
          totalReceivedBytes += stats.receivedBytes;
          totalSentBytes += stats.sentBytes;
        }

        // Save total stats.
        stats.component = "";
        stats.receivedBytes = totalReceivedBytes;
        stats.sentBytes = totalSentBytes;
        aStore.put(stats);
        if (DEBUG) {
          debug("Save network stats: " + JSON.stringify(stats));
        }
      });
    }, aResultCb);
  },

  /**
   * aRecordArray contains an array of json objects storing power stats.
   * The structure of the json object =
   *  {
   *    appId: XX,
   *    serviceType: "XX",
   *    componentStats: {
   *      "component_1": XX, // consumedPower
   *      "component_2": XX,
   *      ...
   *    },
   *  }
   */
  savePowerStats: function(aRecordArray, aTimestamp, aResultCb) {
    if (DEBUG) {
      debug("savePowerStats()");
    }
    let offset = (new Date()).getTimezoneOffset() * 60 * 1000;
    let timestamp = this._normalizeTime(aTimestamp, offset);

    this._dbNewTxn(POWER_STATS_STORE, "readwrite", function(aTxn, aStore) {
      aRecordArray.forEach(function(aRecord) {
        let stats = {
          appId: aRecord.appId,
          serviceType: aRecord.serviceType,
          component: "",
          timestamp: timestamp,
          consumedPower: aRecord.totalStats
        };

        let totalConsumedPower = 0;

        // Save stats of each component to database.
        let data = aRecord.componentStats;
        for (let component in data) {
          // Save stats to database.
          stats.component = component;
          stats.consumedPower = data[component];
          aStore.put(stats);
          if (DEBUG) {
            debug("Save power stats: " + JSON.stringify(stats));
          }
          // Accumulated to total stats.
          totalConsumedPower += stats.consumedPower;
        }

        // Save total stats.
        stats.component = "";
        stats.consumedPower = totalConsumedPower;
        aStore.put(stats);
        if (DEBUG) {
          debug("Save power stats: " + JSON.stringify(stats));
        }
      });
    }, aResultCb);
  },

  // Get stats from a store.
  getStats: function(aType, aManifestURL, aServiceType, aComponent,
                     aStart, aEnd, aResultCb) {
    if (DEBUG) {
      debug(aType + "Mgr.getStats()");
    }

    let offset = (new Date()).getTimezoneOffset() * 60 * 1000;

    // Get appId and check whether manifestURL is a valid app.
    let appId = 0;
    if (aManifestURL) {
      appId = appsService.getAppLocalIdByManifestURL(aManifestURL);

      if (!appId) {
        aResultCb("Invalid manifestURL", null);
        return;
      }
    }

    // Get store name.
    let storeName = statsStoreNames[aType];

    // Normalize start time and end time to SAMPLE_RATE precission.
    let start = this._normalizeTime(aStart, offset);
    let end = this._normalizeTime(aEnd, offset);
    if (DEBUG) {
      debug("Query time range: " + start + " to " + end);
      debug("[appId, serviceType, component] = [" + appId + ", " + aServiceType
            + ", " + aComponent + "]");
    }

    // Create filters.
    let lowerFilter = [appId, aServiceType, aComponent, start];
    let upperFilter = [appId, aServiceType, aComponent, end];

    // Execute DB query.
    this._dbNewTxn(storeName, "readonly", function(aTxn, aStore) {
      let range = IDBKeyRange.bound(lowerFilter, upperFilter, false, false);

      let statsData = [];

      if (!aTxn.result) {
        aTxn.result = Object.create(null);
      }
      aTxn.result.type = aType;
      aTxn.result.component = aComponent;
      aTxn.result.serviceType = aServiceType;
      aTxn.result.manifestURL = aManifestURL;
      aTxn.result.start = start + offset;
      aTxn.result.end = end + offset;
      // Since ResourceStats() would require SAMPLE_RATE when filling the empty
      // entries of statsData array, we append SAMPLE_RATE to the result field
      // to save an IPC call.
      aTxn.result.sampleRate = SAMPLE_RATE;

      let request = aStore.openCursor(range, "prev");
      if (aType == "power") {
        request.onsuccess = function(aEvent) {
          var cursor = aEvent.target.result;
          if (cursor) {
            if (DEBUG) {
              debug("Get " + JSON.stringify(cursor.value));
            }

            // Covert timestamp to current timezone.
            statsData.push({
              timestamp: cursor.value.timestamp + offset,
              consumedPower: cursor.value.consumedPower
            });
            cursor.continue();
            return;
          }
          aTxn.result.statsData = statsData;
        };
      } else if (aType == "network") {
        request.onsuccess = function(aEvent) {
          var cursor = aEvent.target.result;
          if (cursor) {
            if (DEBUG) {
              debug("Get " + JSON.stringify(cursor.value));
            }

            // Covert timestamp to current timezone.
            statsData.push({
              timestamp: cursor.value.timestamp + offset,
              receivedBytes: cursor.value.receivedBytes,
              sentBytes: cursor.value.sentBytes
            });
            cursor.continue();
            return;
          }
          aTxn.result.statsData = statsData;
        };
      }
    }, aResultCb);
  },

  // Delete the stats of a specific app/service (within a specified time range).
  clearStats: function(aType, aAppId, aServiceType, aComponent,
                       aStart, aEnd, aResultCb) {
    if (DEBUG) {
      debug(aType + "Mgr.clearStats()");
    }

    let offset = (new Date()).getTimezoneOffset() * 60 * 1000;

    // Get store name.
    let storeName = statsStoreNames[aType];

    // Normalize start and end time to SAMPLE_RATE precission.
    let start = this._normalizeTime(aStart, offset);
    let end = this._normalizeTime(aEnd, offset);
    if (DEBUG) {
      debug("Query time range: " + start + " to " + end);
      debug("[appId, serviceType, component] = [" + aAppId + ", " + aServiceType
            + ", " + aComponent + "]");
    }

    // Create filters.
    let lowerFilter = [aAppId, aServiceType, aComponent, start];
    let upperFilter = [aAppId, aServiceType, aComponent, end];

    // Execute clear operation.
    this._dbNewTxn(storeName, "readwrite", function(aTxn, aStore) {
      let range = IDBKeyRange.bound(lowerFilter, upperFilter, false, false);
      let request = aStore.openCursor(range).onsuccess = function(aEvent) {
        let cursor = aEvent.target.result;
        if (cursor) {
          if (DEBUG) {
            debug("Delete " + JSON.stringify(cursor.value) + " from database");
          }
          cursor.delete();
          cursor.continue();
          return;
        }
      };
    }, aResultCb);
  },

  // Delete all stats saved in a store.
  clearAllStats: function(aType, aResultCb) {
    if (DEBUG) {
      debug(aType + "Mgr.clearAllStats()");
    }

    let storeName = statsStoreNames[aType];

    // Execute clear operation.
    this._dbNewTxn(storeName, "readwrite", function(aTxn, aStore) {
      if (DEBUG) {
        debug("Clear " + aType + " stats from datastore");
      }
      aStore.clear();
    }, aResultCb);
  },

  addAlarm: function(aAlarm, aResultCb) {
    if (DEBUG) {
      debug(aAlarm.type + "Mgr.addAlarm()");
      debug("alarm = " + JSON.stringify(aAlarm));
    }

    this._dbNewTxn(ALARM_STORE, "readwrite", function(aTxn, aStore) {
      aStore.put(aAlarm).onsuccess = function setResult(aEvent) {
        // Get alarmId.
        aTxn.result = aEvent.target.result;
        if (DEBUG) {
          debug("New alarm ID: " + aTxn.result);
        }
      };
    }, aResultCb);
  },

  // Convert DB record to alarm object.
  _recordToAlarm: function(aRecord) {
    let alarm = {
      alarmId: aRecord.alarmId,
      type: aRecord.type,
      component: aRecord.component,
      serviceType: aRecord.serviceType,
      manifestURL: aRecord.manifestURL,
      threshold: aRecord.threshold,
      data: aRecord.data
    };

    return alarm;
  },

  getAlarms: function(aType, aOptions, aResultCb) {
    if (DEBUG) {
      debug(aType + "Mgr.getAlarms()");
      debug("[appId, serviceType, component] = [" + aOptions.appId + ", "
            + aOptions.serviceType + ", " + aOptions.component + "]");
    }

    // Execute clear operation.
    this._dbNewTxn(ALARM_STORE, "readwrite", function(aTxn, aStore) {
      if (!aTxn.result) {
        aTxn.result = [];
      }

      let indexName = null;
      let range = null;

      if (aOptions) { // Get alarms associated to specified statsOptions.
        indexName = "controlTarget";
        range = IDBKeyRange.only([aType, aOptions.manifestURL,
                                  aOptions.serviceType, aOptions.component]);
      } else { // Get all alarms of the specified type.
        indexName = "type";
        range = IDBKeyRange.only(aType);
      }

      let request = aStore.index(indexName).openCursor(range);
      request.onsuccess = function onsuccess(aEvent) {
        let cursor = aEvent.target.result;
        if (cursor) {
          aTxn.result.push(this._recordToAlarm(cursor.value));
          cursor.continue();
          return;
        }
      }.bind(this);
    }.bind(this), aResultCb);
  },

  removeAlarm: function(aType, aAlarmId, aResultCb) {
    if (DEBUG) {
      debug("removeAlarms(" + aAlarmId + ")");
    }

    // Execute clear operation.
    this._dbNewTxn(ALARM_STORE, "readwrite", function(aTxn, aStore) {
      aStore.get(aAlarmId).onsuccess = function onsuccess(aEvent) {
        let alarm = aEvent.target.result;
        aTxn.result = false;
        if (!alarm || aType !== alarm.type) {
          return;
        }

        if (DEBUG) {
          debug("Remove alarm " + JSON.stringify(alarm) + " from datastore");
        }
        aStore.delete(aAlarmId);
        aTxn.result = true;
      };
    }, aResultCb);
  },

  removeAllAlarms: function(aType, aResultCb) {
    if (DEBUG) {
      debug(aType + "Mgr.removeAllAlarms()");
    }

    // Execute clear operation.
    this._dbNewTxn(ALARM_STORE, "readwrite", function(aTxn, aStore) {
      if (DEBUG) {
        debug("Remove all " + aType + " alarms from datastore.");
      }

      let range = IDBKeyRange.only(aType);
      let request = aStore.index("type").openCursor(range);
      request.onsuccess = function onsuccess(aEvent) {
        let cursor = aEvent.target.result;
        if (cursor) {
          if (DEBUG) {
            debug("Remove " + JSON.stringify(cursor.value) + " from database.");
          }
          cursor.delete();
          cursor.continue();
          return;
        }
      };
    }, aResultCb);
  },

  // Get all index keys of the component.
  getComponents: function(aType, aResultCb) {
    if (DEBUG) {
      debug(aType + "Mgr.getComponents()");
    }

    let storeName = statsStoreNames[aType];

    this._dbNewTxn(storeName, "readonly", function(aTxn, aStore) {
      if (!aTxn.result) {
        aTxn.result = [];
      }

      let request = aStore.index("component").openKeyCursor(null, "nextunique");
      request.onsuccess = function onsuccess(aEvent) {
        let cursor = aEvent.target.result;
        if (cursor) {
          aTxn.result.push(cursor.key);
          cursor.continue();
          return;
        }

        // Remove "" from the result, which indicates sum of all
        // components' stats.
        let index = aTxn.result.indexOf("");
        if (index > -1) {
          aTxn.result.splice(index, 1);
        }
      };
    }, aResultCb);
  },

  get sampleRate () {
    return SAMPLE_RATE;
  },

  get maxStorageAge() {
    return MAX_STORAGE_AGE;
  },
};

