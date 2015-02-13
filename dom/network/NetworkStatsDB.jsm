/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ['NetworkStatsDB'];

const DEBUG = false;
function debug(s) { dump("-*- NetworkStatsDB: " + s + "\n"); }

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/IndexedDBHelper.jsm");
Cu.importGlobalProperties(["indexedDB"]);

XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

const DB_NAME = "net_stats";
const DB_VERSION = 9;
const DEPRECATED_STATS_STORE_NAME =
  [
    "net_stats_v2",    // existed only in DB version 2
    "net_stats",       // existed in DB version 1 and 3 to 5
    "net_stats_store", // existed in DB version 6 to 8
  ];
const STATS_STORE_NAME = "net_stats_store_v3"; // since DB version 9
const ALARMS_STORE_NAME = "net_alarm";

// Constant defining the maximum values allowed per interface. If more, older
// will be erased.
const VALUES_MAX_LENGTH = 6 * 30;

// Constant defining the rate of the samples. Daily.
const SAMPLE_RATE = 1000 * 60 * 60 * 24;

this.NetworkStatsDB = function NetworkStatsDB() {
  if (DEBUG) {
    debug("Constructor");
  }
  this.initDBHelper(DB_NAME, DB_VERSION, [STATS_STORE_NAME, ALARMS_STORE_NAME]);
}

NetworkStatsDB.prototype = {
  __proto__: IndexedDBHelper.prototype,

  dbNewTxn: function dbNewTxn(store_name, txn_type, callback, txnCb) {
    function successCb(result) {
      txnCb(null, result);
    }
    function errorCb(error) {
      txnCb(error, null);
    }
    return this.newTxn(txn_type, store_name, callback, successCb, errorCb);
  },

  /**
   * The onupgradeneeded handler of the IDBOpenDBRequest.
   * This function is called in IndexedDBHelper open() method.
   *
   * @param {IDBTransaction} aTransaction
   *        {IDBDatabase} aDb
   *        {64-bit integer} aOldVersion The version number on local storage.
   *        {64-bit integer} aNewVersion The version number to be upgraded to.
   *
   * @note  Be careful with the database upgrade pattern.
   *        Because IndexedDB operations are performed asynchronously, we must
   *        apply a recursive approach instead of an iterative approach while
   *        upgrading versions.
   */
  upgradeSchema: function upgradeSchema(aTransaction, aDb, aOldVersion, aNewVersion) {
    if (DEBUG) {
      debug("upgrade schema from: " + aOldVersion + " to " + aNewVersion + " called!");
    }
    let db = aDb;
    let objectStore;

    // An array of upgrade functions for each version.
    let upgradeSteps = [
      function upgrade0to1() {
        if (DEBUG) debug("Upgrade 0 to 1: Create object stores and indexes.");

        // Create the initial database schema.
        objectStore = db.createObjectStore(DEPRECATED_STATS_STORE_NAME[1],
                                           { keyPath: ["connectionType", "timestamp"] });
        objectStore.createIndex("connectionType", "connectionType", { unique: false });
        objectStore.createIndex("timestamp", "timestamp", { unique: false });
        objectStore.createIndex("rxBytes", "rxBytes", { unique: false });
        objectStore.createIndex("txBytes", "txBytes", { unique: false });
        objectStore.createIndex("rxTotalBytes", "rxTotalBytes", { unique: false });
        objectStore.createIndex("txTotalBytes", "txTotalBytes", { unique: false });

        upgradeNextVersion();
      },

      function upgrade1to2() {
        if (DEBUG) debug("Upgrade 1 to 2: Do nothing.");
        upgradeNextVersion();
      },

      function upgrade2to3() {
        if (DEBUG) debug("Upgrade 2 to 3: Add keyPath appId to object store.");

        // In order to support per-app traffic data storage, the original
        // objectStore needs to be replaced by a new objectStore with new
        // key path ("appId") and new index ("appId").
        // Also, since now networks are identified by their
        // [networkId, networkType] not just by their connectionType,
        // to modify the keyPath is mandatory to delete the object store
        // and create it again. Old data is going to be deleted because the
        // networkId for each sample can not be set.

        // In version 1.2 objectStore name was 'net_stats_v2', to avoid errors when
        // upgrading from 1.2 to 1.3 objectStore name should be checked.
        let stores = db.objectStoreNames;
        let deprecatedName = DEPRECATED_STATS_STORE_NAME[0];
        let storeName = DEPRECATED_STATS_STORE_NAME[1];
        if(stores.contains(deprecatedName)) {
          // Delete the obsolete stats store.
          db.deleteObjectStore(deprecatedName);
        } else {
          // Re-create stats object store without copying records.
          db.deleteObjectStore(storeName);
        }

        objectStore = db.createObjectStore(storeName, { keyPath: ["appId", "network", "timestamp"] });
        objectStore.createIndex("appId", "appId", { unique: false });
        objectStore.createIndex("network", "network", { unique: false });
        objectStore.createIndex("networkType", "networkType", { unique: false });
        objectStore.createIndex("timestamp", "timestamp", { unique: false });
        objectStore.createIndex("rxBytes", "rxBytes", { unique: false });
        objectStore.createIndex("txBytes", "txBytes", { unique: false });
        objectStore.createIndex("rxTotalBytes", "rxTotalBytes", { unique: false });
        objectStore.createIndex("txTotalBytes", "txTotalBytes", { unique: false });

        upgradeNextVersion();
      },

      function upgrade3to4() {
        if (DEBUG) debug("Upgrade 3 to 4: Delete redundant indexes.");

        // Delete redundant indexes (leave "network" only).
        objectStore = aTransaction.objectStore(DEPRECATED_STATS_STORE_NAME[1]);
        if (objectStore.indexNames.contains("appId")) {
          objectStore.deleteIndex("appId");
        }
        if (objectStore.indexNames.contains("networkType")) {
          objectStore.deleteIndex("networkType");
        }
        if (objectStore.indexNames.contains("timestamp")) {
          objectStore.deleteIndex("timestamp");
        }
        if (objectStore.indexNames.contains("rxBytes")) {
          objectStore.deleteIndex("rxBytes");
        }
        if (objectStore.indexNames.contains("txBytes")) {
          objectStore.deleteIndex("txBytes");
        }
        if (objectStore.indexNames.contains("rxTotalBytes")) {
          objectStore.deleteIndex("rxTotalBytes");
        }
        if (objectStore.indexNames.contains("txTotalBytes")) {
          objectStore.deleteIndex("txTotalBytes");
        }

        upgradeNextVersion();
      },

      function upgrade4to5() {
        if (DEBUG) debug("Upgrade 4 to 5: Create object store for alarms.");

        // In order to manage alarms, it is necessary to use a global counter
        // (totalBytes) that will increase regardless of the system reboot.
        objectStore = aTransaction.objectStore(DEPRECATED_STATS_STORE_NAME[1]);

        // Now, systemBytes will hold the old totalBytes and totalBytes will
        // keep the increasing counter. |counters| will keep the track of
        // accumulated values.
        let counters = {};

        objectStore.openCursor().onsuccess = function(event) {
          let cursor = event.target.result;
          if (!cursor){
            // upgrade4to5 completed now.
            upgradeNextVersion();
            return;
          }

          cursor.value.rxSystemBytes = cursor.value.rxTotalBytes;
          cursor.value.txSystemBytes = cursor.value.txTotalBytes;

          if (cursor.value.appId == 0) {
            let netId = cursor.value.network[0] + '' + cursor.value.network[1];
            if (!counters[netId]) {
              counters[netId] = {
                rxCounter: 0,
                txCounter: 0,
                lastRx: 0,
                lastTx: 0
              };
            }

            let rxDiff = cursor.value.rxSystemBytes - counters[netId].lastRx;
            let txDiff = cursor.value.txSystemBytes - counters[netId].lastTx;
            if (rxDiff < 0 || txDiff < 0) {
              // System reboot between samples, so take the current one.
              rxDiff = cursor.value.rxSystemBytes;
              txDiff = cursor.value.txSystemBytes;
            }

            counters[netId].rxCounter += rxDiff;
            counters[netId].txCounter += txDiff;
            cursor.value.rxTotalBytes = counters[netId].rxCounter;
            cursor.value.txTotalBytes = counters[netId].txCounter;

            counters[netId].lastRx = cursor.value.rxSystemBytes;
            counters[netId].lastTx = cursor.value.txSystemBytes;
          } else {
            cursor.value.rxTotalBytes = cursor.value.rxSystemBytes;
            cursor.value.txTotalBytes = cursor.value.txSystemBytes;
          }

          cursor.update(cursor.value);
          cursor.continue();
        };

        // Create object store for alarms.
        objectStore = db.createObjectStore(ALARMS_STORE_NAME, { keyPath: "id", autoIncrement: true });
        objectStore.createIndex("alarm", ['networkId','threshold'], { unique: false });
        objectStore.createIndex("manifestURL", "manifestURL", { unique: false });
      },

      function upgrade5to6() {
        if (DEBUG) debug("Upgrade 5 to 6: Add keyPath serviceType to object store.");

        // In contrast to "per-app" traffic data, "system-only" traffic data
        // refers to data which can not be identified by any applications.
        // To further support "system-only" data storage, the data can be
        // saved by service type (e.g., Tethering, OTA). Thus it's needed to
        // have a new key ("serviceType") for the ojectStore.
        let newObjectStore;
        let deprecatedName = DEPRECATED_STATS_STORE_NAME[1];
        newObjectStore = db.createObjectStore(DEPRECATED_STATS_STORE_NAME[2],
                         { keyPath: ["appId", "serviceType", "network", "timestamp"] });
        newObjectStore.createIndex("network", "network", { unique: false });

        // Copy the data from the original objectStore to the new objectStore.
        objectStore = aTransaction.objectStore(deprecatedName);
        objectStore.openCursor().onsuccess = function(event) {
          let cursor = event.target.result;
          if (!cursor) {
            db.deleteObjectStore(deprecatedName);
            // upgrade5to6 completed now.
            upgradeNextVersion();
            return;
          }

          let newStats = cursor.value;
          newStats.serviceType = "";
          newObjectStore.put(newStats);
          cursor.continue();
        };
      },

      function upgrade6to7() {
        if (DEBUG) debug("Upgrade 6 to 7: Replace alarm threshold by relativeThreshold.");

        // Replace threshold attribute of alarm index by relativeThreshold in alarms DB.
        // Now alarms are indexed by relativeThreshold, which is the threshold relative
        // to current system stats.
        let alarmsStore = aTransaction.objectStore(ALARMS_STORE_NAME);

        // Delete "alarm" index.
        if (alarmsStore.indexNames.contains("alarm")) {
          alarmsStore.deleteIndex("alarm");
        }

        // Create new "alarm" index.
        alarmsStore.createIndex("alarm", ['networkId','relativeThreshold'], { unique: false });

        // Populate new "alarm" index attributes.
        alarmsStore.openCursor().onsuccess = function(event) {
          let cursor = event.target.result;
          if (!cursor) {
            upgrade6to7_updateTotalBytes();
            return;
          }

          cursor.value.relativeThreshold = cursor.value.threshold;
          cursor.value.absoluteThreshold = cursor.value.threshold;
          delete cursor.value.threshold;

          cursor.update(cursor.value);
          cursor.continue();
        }

        function upgrade6to7_updateTotalBytes() {
          if (DEBUG) debug("Upgrade 6 to 7: Update TotalBytes.");
          // Previous versions save accumulative totalBytes, increasing although the system
          // reboots or resets stats. But is necessary to reset the total counters when reset
          // through 'clearInterfaceStats'.
          let statsStore = aTransaction.objectStore(DEPRECATED_STATS_STORE_NAME[2]);
          let networks = [];

          // Find networks stored in the database.
          statsStore.index("network").openKeyCursor(null, "nextunique").onsuccess = function(event) {
            let cursor = event.target.result;

            // Store each network into an array.
            if (cursor) {
              networks.push(cursor.key);
              cursor.continue();
              return;
            }

            // Start to deal with each network.
            let pending = networks.length;

            if (pending === 0) {
              // Found no records of network. upgrade6to7 completed now.
              upgradeNextVersion();
              return;
            }

            networks.forEach(function(network) {
              let lowerFilter = [0, "", network, 0];
              let upperFilter = [0, "", network, ""];
              let range = IDBKeyRange.bound(lowerFilter, upperFilter, false, false);

              // Find number of samples for a given network.
              statsStore.count(range).onsuccess = function(event) {
                let recordCount = event.target.result;

                // If there are more samples than the max allowed, there is no way to know
                // when does reset take place.
                if (recordCount === 0 || recordCount >= VALUES_MAX_LENGTH) {
                  pending--;
                  if (pending === 0) {
                    upgradeNextVersion();
                  }
                  return;
                }

                let last = null;
                // Reset detected if the first sample totalCounters are different than bytes
                // counters. If so, the total counters should be recalculated.
                statsStore.openCursor(range).onsuccess = function(event) {
                  let cursor = event.target.result;
                  if (!cursor) {
                    pending--;
                    if (pending === 0) {
                      upgradeNextVersion();
                    }
                    return;
                  }
                  if (!last) {
                    if (cursor.value.rxTotalBytes == cursor.value.rxBytes &&
                        cursor.value.txTotalBytes == cursor.value.txBytes) {
                      pending--;
                      if (pending === 0) {
                        upgradeNextVersion();
                      }
                      return;
                    }

                    cursor.value.rxTotalBytes = cursor.value.rxBytes;
                    cursor.value.txTotalBytes = cursor.value.txBytes;
                    cursor.update(cursor.value);
                    last = cursor.value;
                    cursor.continue();
                    return;
                  }

                  // Recalculate the total counter for last / current sample
                  cursor.value.rxTotalBytes = last.rxTotalBytes + cursor.value.rxBytes;
                  cursor.value.txTotalBytes = last.txTotalBytes + cursor.value.txBytes;
                  cursor.update(cursor.value);
                  last = cursor.value;
                  cursor.continue();
                }
              }
            }, this); // end of networks.forEach()
          }; // end of statsStore.index("network").openKeyCursor().onsuccess callback
        } // end of function upgrade6to7_updateTotalBytes
      },

      function upgrade7to8() {
        if (DEBUG) debug("Upgrade 7 to 8: Create index serviceType.");

        // Create index for 'ServiceType' in order to make it retrievable.
        let statsStore = aTransaction.objectStore(DEPRECATED_STATS_STORE_NAME[2]);
        statsStore.createIndex("serviceType", "serviceType", { unique: false });

        upgradeNextVersion();
      },

      function upgrade8to9() {
        if (DEBUG) debug("Upgrade 8 to 9: Add keyPath isInBrowser to " +
                         "network stats object store");

        // Since B2G v2.0, there is no stand-alone browser app anymore.
        // The browser app is a mozbrowser iframe element owned by system app.
        // In order to separate traffic generated from system and browser, we
        // have to add a new attribute |isInBrowser| as keyPath.
        // Refer to bug 1070944 for more detail.
        let newObjectStore;
        let deprecatedName = DEPRECATED_STATS_STORE_NAME[2];
        newObjectStore = db.createObjectStore(STATS_STORE_NAME,
                         { keyPath: ["appId", "isInBrowser", "serviceType",
                                     "network", "timestamp"] });
        newObjectStore.createIndex("network", "network", { unique: false });
        newObjectStore.createIndex("serviceType", "serviceType", { unique: false });

        // Copy records from the current object store to the new one.
        objectStore = aTransaction.objectStore(deprecatedName);
        objectStore.openCursor().onsuccess = function (event) {
          let cursor = event.target.result;
          if (!cursor) {
            db.deleteObjectStore(deprecatedName);
            // upgrade8to9 completed now.
            return;
          }
          let newStats = cursor.value;
          // Augment records by adding the new isInBrowser attribute.
          // Notes:
          // 1. Key value cannot be boolean type. Use 1/0 instead of true/false.
          // 2. Most traffic of system app should come from its browser iframe,
          //    thus assign isInBrowser as 1 for system app.
          let manifestURL = appsService.getManifestURLByLocalId(newStats.appId);
          if (manifestURL && manifestURL.search(/app:\/\/system\./) === 0) {
            newStats.isInBrowser = 1;
          } else {
            newStats.isInBrowser = 0;
          }
          newObjectStore.put(newStats);
          cursor.continue();
        };
      }
    ];

    let index = aOldVersion;
    let outer = this;

    function upgradeNextVersion() {
      if (index == aNewVersion) {
        debug("Upgrade finished.");
        return;
      }

      try {
        var i = index++;
        if (DEBUG) debug("Upgrade step: " + i + "\n");
        upgradeSteps[i].call(outer);
      } catch (ex) {
        dump("Caught exception " + ex);
        throw ex;
        return;
      }
    }

    if (aNewVersion > upgradeSteps.length) {
      debug("No migration steps for the new version!");
      aTransaction.abort();
      return;
    }

    upgradeNextVersion();
  },

  importData: function importData(aStats) {
    let stats = { appId:         aStats.appId,
                  isInBrowser:   aStats.isInBrowser ? 1 : 0,
                  serviceType:   aStats.serviceType,
                  network:       [aStats.networkId, aStats.networkType],
                  timestamp:     aStats.timestamp,
                  rxBytes:       aStats.rxBytes,
                  txBytes:       aStats.txBytes,
                  rxSystemBytes: aStats.rxSystemBytes,
                  txSystemBytes: aStats.txSystemBytes,
                  rxTotalBytes:  aStats.rxTotalBytes,
                  txTotalBytes:  aStats.txTotalBytes };

    return stats;
  },

  exportData: function exportData(aStats) {
    let stats = { appId:        aStats.appId,
                  isInBrowser:  aStats.isInBrowser ? true : false,
                  serviceType:  aStats.serviceType,
                  networkId:    aStats.network[0],
                  networkType:  aStats.network[1],
                  timestamp:    aStats.timestamp,
                  rxBytes:      aStats.rxBytes,
                  txBytes:      aStats.txBytes,
                  rxTotalBytes: aStats.rxTotalBytes,
                  txTotalBytes: aStats.txTotalBytes };

    return stats;
  },

  normalizeDate: function normalizeDate(aDate) {
    // Convert to UTC according to timezone and
    // filter timestamp to get SAMPLE_RATE precission
    let timestamp = aDate.getTime() - aDate.getTimezoneOffset() * 60 * 1000;
    timestamp = Math.floor(timestamp / SAMPLE_RATE) * SAMPLE_RATE;
    return timestamp;
  },

  saveStats: function saveStats(aStats, aResultCb) {
    let isAccumulative = aStats.isAccumulative;
    let timestamp = this.normalizeDate(aStats.date);

    let stats = { appId:         aStats.appId,
                  isInBrowser:   aStats.isInBrowser,
                  serviceType:   aStats.serviceType,
                  networkId:     aStats.networkId,
                  networkType:   aStats.networkType,
                  timestamp:     timestamp,
                  rxBytes:       isAccumulative ? 0 : aStats.rxBytes,
                  txBytes:       isAccumulative ? 0 : aStats.txBytes,
                  rxSystemBytes: isAccumulative ? aStats.rxBytes : 0,
                  txSystemBytes: isAccumulative ? aStats.txBytes : 0,
                  rxTotalBytes:  isAccumulative ? aStats.rxBytes : 0,
                  txTotalBytes:  isAccumulative ? aStats.txBytes : 0 };

    stats = this.importData(stats);

    this.dbNewTxn(STATS_STORE_NAME, "readwrite", function(aTxn, aStore) {
      if (DEBUG) {
        debug("Filtered time: " + new Date(timestamp));
        debug("New stats: " + JSON.stringify(stats));
      }

      let lowerFilter = [stats.appId, stats.isInBrowser, stats.serviceType,
                         stats.network, 0];
      let upperFilter = [stats.appId, stats.isInBrowser, stats.serviceType,
                         stats.network, ""];
      let range = IDBKeyRange.bound(lowerFilter, upperFilter, false, false);

      let request = aStore.openCursor(range, 'prev');
      request.onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (!cursor) {
          // Empty, so save first element.

          if (!isAccumulative) {
            this._saveStats(aTxn, aStore, stats);
            return;
          }

          // There could be a time delay between the point when the network
          // interface comes up and the point when the database is initialized.
          // In this short interval some traffic data are generated but are not
          // registered by the first sample.
          stats.rxBytes = stats.rxTotalBytes;
          stats.txBytes = stats.txTotalBytes;

          // However, if the interface is not switched on after the database is
          // initialized (dual sim use case) stats should be set to 0.
          let req = aStore.index("network").openKeyCursor(null, "nextunique");
          req.onsuccess = function onsuccess(event) {
            let cursor = event.target.result;
            if (cursor) {
              if (cursor.key[1] == stats.network[1]) {
                stats.rxBytes = 0;
                stats.txBytes = 0;
                this._saveStats(aTxn, aStore, stats);
                return;
              }

              cursor.continue();
              return;
            }

            this._saveStats(aTxn, aStore, stats);
          }.bind(this);

          return;
        }

        // There are old samples
        if (DEBUG) {
          debug("Last value " + JSON.stringify(cursor.value));
        }

        // Remove stats previous to now - VALUE_MAX_LENGTH
        this._removeOldStats(aTxn, aStore, stats.appId, stats.isInBrowser,
                             stats.serviceType, stats.network, stats.timestamp);

        // Process stats before save
        this._processSamplesDiff(aTxn, aStore, cursor, stats, isAccumulative);
      }.bind(this);
    }.bind(this), aResultCb);
  },

  /*
   * This function check that stats are saved in the database following the sample rate.
   * In this way is easier to find elements when stats are requested.
   */
  _processSamplesDiff: function _processSamplesDiff(aTxn,
                                                    aStore,
                                                    aLastSampleCursor,
                                                    aNewSample,
                                                    aIsAccumulative) {
    let lastSample = aLastSampleCursor.value;

    // Get difference between last and new sample.
    let diff = (aNewSample.timestamp - lastSample.timestamp) / SAMPLE_RATE;
    if (diff % 1) {
      // diff is decimal, so some error happened because samples are stored as a multiple
      // of SAMPLE_RATE
      aTxn.abort();
      throw new Error("Error processing samples");
    }

    if (DEBUG) {
      debug("New: " + aNewSample.timestamp + " - Last: " +
            lastSample.timestamp + " - diff: " + diff);
    }

    // If the incoming data has a accumulation feature, the new
    // |txBytes|/|rxBytes| is assigend by differnces between the new
    // |txTotalBytes|/|rxTotalBytes| and the last |txTotalBytes|/|rxTotalBytes|.
    // Else, if incoming data is non-accumulative, the |txBytes|/|rxBytes|
    // is the new |txBytes|/|rxBytes|.
    let rxDiff = 0;
    let txDiff = 0;
    if (aIsAccumulative) {
      rxDiff = aNewSample.rxSystemBytes - lastSample.rxSystemBytes;
      txDiff = aNewSample.txSystemBytes - lastSample.txSystemBytes;
      if (rxDiff < 0 || txDiff < 0) {
        rxDiff = aNewSample.rxSystemBytes;
        txDiff = aNewSample.txSystemBytes;
      }
      aNewSample.rxBytes = rxDiff;
      aNewSample.txBytes = txDiff;

      aNewSample.rxTotalBytes = lastSample.rxTotalBytes + rxDiff;
      aNewSample.txTotalBytes = lastSample.txTotalBytes + txDiff;
    } else {
      rxDiff = aNewSample.rxBytes;
      txDiff = aNewSample.txBytes;
    }

    if (diff == 1) {
      // New element.

      // If the incoming data is non-accumulative, the new
      // |rxTotalBytes|/|txTotalBytes| needs to be updated by adding new
      // |rxBytes|/|txBytes| to the last |rxTotalBytes|/|txTotalBytes|.
      if (!aIsAccumulative) {
        aNewSample.rxTotalBytes = aNewSample.rxBytes + lastSample.rxTotalBytes;
        aNewSample.txTotalBytes = aNewSample.txBytes + lastSample.txTotalBytes;
      }

      this._saveStats(aTxn, aStore, aNewSample);
      return;
    }
    if (diff > 1) {
      // Some samples lost. Device off during one or more samplerate periods.
      // Time or timezone changed
      // Add lost samples with 0 bytes and the actual one.
      if (diff > VALUES_MAX_LENGTH) {
        diff = VALUES_MAX_LENGTH;
      }

      let data = [];
      for (let i = diff - 2; i >= 0; i--) {
        let time = aNewSample.timestamp - SAMPLE_RATE * (i + 1);
        let sample = { appId:         aNewSample.appId,
                       isInBrowser:   aNewSample.isInBrowser,
                       serviceType:   aNewSample.serviceType,
                       network:       aNewSample.network,
                       timestamp:     time,
                       rxBytes:       0,
                       txBytes:       0,
                       rxSystemBytes: lastSample.rxSystemBytes,
                       txSystemBytes: lastSample.txSystemBytes,
                       rxTotalBytes:  lastSample.rxTotalBytes,
                       txTotalBytes:  lastSample.txTotalBytes };

        data.push(sample);
      }

      data.push(aNewSample);
      this._saveStats(aTxn, aStore, data);
      return;
    }
    if (diff == 0 || diff < 0) {
      // New element received before samplerate period. It means that device has
      // been restarted (or clock / timezone change).
      // Update element. If diff < 0, clock or timezone changed back. Place data
      // in the last sample.

      // Old |rxTotalBytes|/|txTotalBytes| needs to get updated by adding the
      // last |rxTotalBytes|/|txTotalBytes|.
      lastSample.rxBytes += rxDiff;
      lastSample.txBytes += txDiff;
      lastSample.rxSystemBytes = aNewSample.rxSystemBytes;
      lastSample.txSystemBytes = aNewSample.txSystemBytes;
      lastSample.rxTotalBytes += rxDiff;
      lastSample.txTotalBytes += txDiff;

      if (DEBUG) {
        debug("Update: " + JSON.stringify(lastSample));
      }
      let req = aLastSampleCursor.update(lastSample);
    }
  },

  _saveStats: function _saveStats(aTxn, aStore, aNetworkStats) {
    if (DEBUG) {
      debug("_saveStats: " + JSON.stringify(aNetworkStats));
    }

    if (Array.isArray(aNetworkStats)) {
      let len = aNetworkStats.length - 1;
      for (let i = 0; i <= len; i++) {
        aStore.put(aNetworkStats[i]);
      }
    } else {
      aStore.put(aNetworkStats);
    }
  },

  _removeOldStats: function _removeOldStats(aTxn, aStore, aAppId, aIsInBrowser,
                                            aServiceType, aNetwork, aDate) {
    // Callback function to remove old items when new ones are added.
    let filterDate = aDate - (SAMPLE_RATE * VALUES_MAX_LENGTH - 1);
    let lowerFilter = [aAppId, aIsInBrowser, aServiceType, aNetwork, 0];
    let upperFilter = [aAppId, aIsInBrowser, aServiceType, aNetwork, filterDate];
    let range = IDBKeyRange.bound(lowerFilter, upperFilter, false, false);
    let lastSample = null;
    let self = this;

    aStore.openCursor(range).onsuccess = function(event) {
      var cursor = event.target.result;
      if (cursor) {
        lastSample = cursor.value;
        cursor.delete();
        cursor.continue();
        return;
      }

      // If all samples for a network are removed, an empty sample
      // has to be saved to keep the totalBytes in order to compute
      // future samples because system counters are not set to 0.
      // Thus, if there are no samples left, the last sample removed
      // will be saved again after setting its bytes to 0.
      let request = aStore.index("network").openCursor(aNetwork);
      request.onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (!cursor && lastSample != null) {
          let timestamp = new Date();
          timestamp = self.normalizeDate(timestamp);
          lastSample.timestamp = timestamp;
          lastSample.rxBytes = 0;
          lastSample.txBytes = 0;
          self._saveStats(aTxn, aStore, lastSample);
        }
      };
    };
  },

  clearInterfaceStats: function clearInterfaceStats(aNetwork, aResultCb) {
    let network = [aNetwork.network.id, aNetwork.network.type];
    let self = this;

    // Clear and save an empty sample to keep sync with system counters
    this.dbNewTxn(STATS_STORE_NAME, "readwrite", function(aTxn, aStore) {
      let sample = null;
      let request = aStore.index("network").openCursor(network, "prev");
      request.onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (cursor) {
          if (!sample && cursor.value.appId == 0) {
            sample = cursor.value;
          }

          cursor.delete();
          cursor.continue();
          return;
        }

        if (sample) {
          let timestamp = new Date();
          timestamp = self.normalizeDate(timestamp);
          sample.timestamp = timestamp;
          sample.appId = 0;
          sample.isInBrowser = 0;
          sample.serviceType = "";
          sample.rxBytes = 0;
          sample.txBytes = 0;
          sample.rxTotalBytes = 0;
          sample.txTotalBytes = 0;

          self._saveStats(aTxn, aStore, sample);
        }
      };
    }, this._resetAlarms.bind(this, aNetwork.networkId, aResultCb));
  },

  clearStats: function clearStats(aNetworks, aResultCb) {
    let index = 0;
    let stats = [];
    let self = this;

    let callback = function(aError, aResult) {
      index++;

      if (!aError && index < aNetworks.length) {
        self.clearInterfaceStats(aNetworks[index], callback);
        return;
      }

      aResultCb(aError, aResult);
    };

    if (!aNetworks[index]) {
      aResultCb(null, true);
      return;
    }
    this.clearInterfaceStats(aNetworks[index], callback);
  },

  getCurrentStats: function getCurrentStats(aNetwork, aDate, aResultCb) {
    if (DEBUG) {
      debug("Get current stats for " + JSON.stringify(aNetwork) + " since " + aDate);
    }

    let network = [aNetwork.id, aNetwork.type];
    if (aDate) {
      this._getCurrentStatsFromDate(network, aDate, aResultCb);
      return;
    }

    this._getCurrentStats(network, aResultCb);
  },

  _getCurrentStats: function _getCurrentStats(aNetwork, aResultCb) {
    this.dbNewTxn(STATS_STORE_NAME, "readonly", function(txn, store) {
      let request = null;
      let upperFilter = [0, 1, "", aNetwork, Date.now()];
      let range = IDBKeyRange.upperBound(upperFilter, false);
      let result = { rxBytes:      0, txBytes:      0,
                     rxTotalBytes: 0, txTotalBytes: 0 };

      request = store.openCursor(range, "prev");

      request.onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (cursor) {
          result.rxBytes = result.rxTotalBytes = cursor.value.rxTotalBytes;
          result.txBytes = result.txTotalBytes = cursor.value.txTotalBytes;
        }

        txn.result = result;
      };
    }.bind(this), aResultCb);
  },

  _getCurrentStatsFromDate: function _getCurrentStatsFromDate(aNetwork, aDate, aResultCb) {
    aDate = new Date(aDate);
    this.dbNewTxn(STATS_STORE_NAME, "readonly", function(txn, store) {
      let request = null;
      let start = this.normalizeDate(aDate);
      let upperFilter = [0, 1, "", aNetwork, Date.now()];
      let range = IDBKeyRange.upperBound(upperFilter, false);
      let result = { rxBytes:      0, txBytes:      0,
                     rxTotalBytes: 0, txTotalBytes: 0 };

      request = store.openCursor(range, "prev");

      request.onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (cursor) {
          result.rxBytes = result.rxTotalBytes = cursor.value.rxTotalBytes;
          result.txBytes = result.txTotalBytes = cursor.value.txTotalBytes;
        }

        let timestamp = cursor.value.timestamp;
        let range = IDBKeyRange.lowerBound(lowerFilter, false);
        request = store.openCursor(range);

        request.onsuccess = function onsuccess(event) {
          let cursor = event.target.result;
          if (cursor) {
            if (cursor.value.timestamp == timestamp) {
              // There is one sample only.
              result.rxBytes = cursor.value.rxBytes;
              result.txBytes = cursor.value.txBytes;
            } else {
              result.rxBytes -= cursor.value.rxTotalBytes;
              result.txBytes -= cursor.value.txTotalBytes;
            }
          }

          txn.result = result;
        };
      };
    }.bind(this), aResultCb);
  },

  find: function find(aResultCb, aAppId, aBrowsingTrafficOnly, aServiceType,
                      aNetwork, aStart, aEnd, aAppManifestURL) {
    let offset = (new Date()).getTimezoneOffset() * 60 * 1000;
    let start = this.normalizeDate(aStart);
    let end = this.normalizeDate(aEnd);

    if (DEBUG) {
      debug("Find samples for appId: " + aAppId +
            " browsingTrafficOnly: " + aBrowsingTrafficOnly +
            " serviceType: " + aServiceType +
            " network: " + JSON.stringify(aNetwork) + " from " + start +
            " until " + end);
      debug("Start time: " + new Date(start));
      debug("End time: " + new Date(end));
    }

    // Find samples of browsing traffic (isInBrowser = 1) first since they are
    // needed no matter browsingTrafficOnly is true or false.
    // We have to make two queries to database because we cannot filter correct
    // records by a single query that sets ranges for two keys (isInBrowser and
    // timestamp). We think it is because the keyPath contains an array
    // (network) so such query does not work.
    this.dbNewTxn(STATS_STORE_NAME, "readonly", function(aTxn, aStore) {
      let network = [aNetwork.id, aNetwork.type];
      let lowerFilter = [aAppId, 1, aServiceType, network, start];
      let upperFilter = [aAppId, 1, aServiceType, network, end];
      let range = IDBKeyRange.bound(lowerFilter, upperFilter, false, false);

      let data = [];

      if (!aTxn.result) {
        aTxn.result = {};
      }
      aTxn.result.appManifestURL = aAppManifestURL;
      aTxn.result.browsingTrafficOnly = aBrowsingTrafficOnly;
      aTxn.result.serviceType = aServiceType;
      aTxn.result.network = aNetwork;
      aTxn.result.start = aStart;
      aTxn.result.end = aEnd;

      let request = aStore.openCursor(range).onsuccess = function(event) {
        var cursor = event.target.result;
        if (cursor){
          data.push({ rxBytes: cursor.value.rxBytes,
                      txBytes: cursor.value.txBytes,
                      date: new Date(cursor.value.timestamp + offset) });
          cursor.continue();
          return;
        }

        if (aBrowsingTrafficOnly) {
          this.fillResultSamples(start + offset, end + offset, data);
          aTxn.result.data = data;
          return;
        }

        // Find samples of app traffic (isInBrowser = 0) as well if
        // browsingTrafficOnly is false.
        lowerFilter = [aAppId, 0, aServiceType, network, start];
        upperFilter = [aAppId, 0, aServiceType, network, end];
        range = IDBKeyRange.bound(lowerFilter, upperFilter, false, false);
        request = aStore.openCursor(range).onsuccess = function(event) {
          cursor = event.target.result;
          if (cursor) {
            var date = new Date(cursor.value.timestamp + offset);
            var foundData = data.find(function (element, index, array) {
              if (element.date.getTime() !== date.getTime()) {
                return false;
              }
              return element;
            }, date);

            if (foundData) {
              foundData.rxBytes += cursor.value.rxBytes;
              foundData.txBytes += cursor.value.txBytes;
            } else {
              data.push({ rxBytes: cursor.value.rxBytes,
                          txBytes: cursor.value.txBytes,
                          date: new Date(cursor.value.timestamp + offset) });
            }
            cursor.continue();
            return;
          }
          this.fillResultSamples(start + offset, end + offset, data);
          aTxn.result.data = data;
        }.bind(this);  // openCursor(range).onsuccess() callback
      }.bind(this);  // openCursor(range).onsuccess() callback
    }.bind(this), aResultCb);
  },

  /*
   * Fill data array (samples from database) with empty samples to match
   * requested start / end dates.
   */
  fillResultSamples: function fillResultSamples(aStart, aEnd, aData) {
    if (aData.length == 0) {
      aData.push({ rxBytes: undefined,
                  txBytes: undefined,
                  date: new Date(aStart) });
    }

    while (aStart < aData[0].date.getTime()) {
      aData.unshift({ rxBytes: undefined,
                      txBytes: undefined,
                      date: new Date(aData[0].date.getTime() - SAMPLE_RATE) });
    }

    while (aEnd > aData[aData.length - 1].date.getTime()) {
      aData.push({ rxBytes: undefined,
                   txBytes: undefined,
                   date: new Date(aData[aData.length - 1].date.getTime() + SAMPLE_RATE) });
    }
  },

  getAvailableNetworks: function getAvailableNetworks(aResultCb) {
    this.dbNewTxn(STATS_STORE_NAME, "readonly", function(aTxn, aStore) {
      if (!aTxn.result) {
        aTxn.result = [];
      }

      let request = aStore.index("network").openKeyCursor(null, "nextunique");
      request.onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (cursor) {
          aTxn.result.push({ id: cursor.key[0],
                             type: cursor.key[1] });
          cursor.continue();
          return;
        }
      };
    }, aResultCb);
  },

  isNetworkAvailable: function isNetworkAvailable(aNetwork, aResultCb) {
    this.dbNewTxn(STATS_STORE_NAME, "readonly", function(aTxn, aStore) {
      if (!aTxn.result) {
        aTxn.result = false;
      }

      let network = [aNetwork.id, aNetwork.type];
      let request = aStore.index("network").openKeyCursor(IDBKeyRange.only(network));
      request.onsuccess = function onsuccess(event) {
        if (event.target.result) {
          aTxn.result = true;
        }
      };
    }, aResultCb);
  },

  getAvailableServiceTypes: function getAvailableServiceTypes(aResultCb) {
    this.dbNewTxn(STATS_STORE_NAME, "readonly", function(aTxn, aStore) {
      if (!aTxn.result) {
        aTxn.result = [];
      }

      let request = aStore.index("serviceType").openKeyCursor(null, "nextunique");
      request.onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (cursor && cursor.key != "") {
          aTxn.result.push({ serviceType: cursor.key });
          cursor.continue();
          return;
        }
      };
    }, aResultCb);
  },

  get sampleRate () {
    return SAMPLE_RATE;
  },

  get maxStorageSamples () {
    return VALUES_MAX_LENGTH;
  },

  logAllRecords: function logAllRecords(aResultCb) {
    this.dbNewTxn(STATS_STORE_NAME, "readonly", function(aTxn, aStore) {
      aStore.mozGetAll().onsuccess = function onsuccess(event) {
        aTxn.result = event.target.result;
      };
    }, aResultCb);
  },

  alarmToRecord: function alarmToRecord(aAlarm) {
    let record = { networkId: aAlarm.networkId,
                   absoluteThreshold: aAlarm.absoluteThreshold,
                   relativeThreshold: aAlarm.relativeThreshold,
                   startTime: aAlarm.startTime,
                   data: aAlarm.data,
                   manifestURL: aAlarm.manifestURL,
                   pageURL: aAlarm.pageURL };

    if (aAlarm.id) {
      record.id = aAlarm.id;
    }

    return record;
  },

  recordToAlarm: function recordToalarm(aRecord) {
    let alarm = { networkId: aRecord.networkId,
                  absoluteThreshold: aRecord.absoluteThreshold,
                  relativeThreshold: aRecord.relativeThreshold,
                  startTime: aRecord.startTime,
                  data: aRecord.data,
                  manifestURL: aRecord.manifestURL,
                  pageURL: aRecord.pageURL };

    if (aRecord.id) {
      alarm.id = aRecord.id;
    }

    return alarm;
  },

  addAlarm: function addAlarm(aAlarm, aResultCb) {
    this.dbNewTxn(ALARMS_STORE_NAME, "readwrite", function(txn, store) {
      if (DEBUG) {
        debug("Going to add " + JSON.stringify(aAlarm));
      }

      let record = this.alarmToRecord(aAlarm);
      store.put(record).onsuccess = function setResult(aEvent) {
        txn.result = aEvent.target.result;
        if (DEBUG) {
          debug("Request successful. New record ID: " + txn.result);
        }
      };
    }.bind(this), aResultCb);
  },

  getFirstAlarm: function getFirstAlarm(aNetworkId, aResultCb) {
    let self = this;

    this.dbNewTxn(ALARMS_STORE_NAME, "readonly", function(txn, store) {
      if (DEBUG) {
        debug("Get first alarm for network " + aNetworkId);
      }

      let lowerFilter = [aNetworkId, 0];
      let upperFilter = [aNetworkId, ""];
      let range = IDBKeyRange.bound(lowerFilter, upperFilter);

      store.index("alarm").openCursor(range).onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        txn.result = null;
        if (cursor) {
          txn.result = self.recordToAlarm(cursor.value);
        }
      };
    }, aResultCb);
  },

  removeAlarm: function removeAlarm(aAlarmId, aManifestURL, aResultCb) {
    this.dbNewTxn(ALARMS_STORE_NAME, "readwrite", function(txn, store) {
      if (DEBUG) {
        debug("Remove alarm " + aAlarmId);
      }

      store.get(aAlarmId).onsuccess = function onsuccess(event) {
        let record = event.target.result;
        txn.result = false;
        if (!record || (aManifestURL && record.manifestURL != aManifestURL)) {
          return;
        }

        store.delete(aAlarmId);
        txn.result = true;
      }
    }, aResultCb);
  },

  removeAlarms: function removeAlarms(aManifestURL, aResultCb) {
    this.dbNewTxn(ALARMS_STORE_NAME, "readwrite", function(txn, store) {
      if (DEBUG) {
        debug("Remove alarms of " + aManifestURL);
      }

      store.index("manifestURL").openCursor(aManifestURL)
                                .onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (cursor) {
          cursor.delete();
          cursor.continue();
        }
      }
    }, aResultCb);
  },

  updateAlarm: function updateAlarm(aAlarm, aResultCb) {
    let self = this;
    this.dbNewTxn(ALARMS_STORE_NAME, "readwrite", function(txn, store) {
      if (DEBUG) {
        debug("Update alarm " + aAlarm.id);
      }

      let record = self.alarmToRecord(aAlarm);
      store.openCursor(record.id).onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        txn.result = false;
        if (cursor) {
          cursor.update(record);
          txn.result = true;
        }
      }
    }, aResultCb);
  },

  getAlarms: function getAlarms(aNetworkId, aManifestURL, aResultCb) {
    let self = this;
    this.dbNewTxn(ALARMS_STORE_NAME, "readonly", function(txn, store) {
      if (DEBUG) {
        debug("Get alarms for " + aManifestURL);
      }

      txn.result = [];
      store.index("manifestURL").openCursor(aManifestURL)
                                .onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (!cursor) {
          return;
        }

        if (!aNetworkId || cursor.value.networkId == aNetworkId) {
          txn.result.push(self.recordToAlarm(cursor.value));
        }

        cursor.continue();
      }
    }, aResultCb);
  },

  _resetAlarms: function _resetAlarms(aNetworkId, aResultCb) {
    this.dbNewTxn(ALARMS_STORE_NAME, "readwrite", function(txn, store) {
      if (DEBUG) {
        debug("Reset alarms for network " + aNetworkId);
      }

      let lowerFilter = [aNetworkId, 0];
      let upperFilter = [aNetworkId, ""];
      let range = IDBKeyRange.bound(lowerFilter, upperFilter);

      store.index("alarm").openCursor(range).onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (cursor) {
          if (cursor.value.startTime) {
            cursor.value.relativeThreshold = cursor.value.threshold;
            cursor.update(cursor.value);
          }
          cursor.continue();
          return;
        }
      };
    }, aResultCb);
  }
};
