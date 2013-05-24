/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ['NetworkStatsDB'];

const DEBUG = false;
function debug(s) { dump("-*- NetworkStatsDB: " + s + "\n"); }

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/IndexedDBHelper.jsm");

const DB_NAME = "net_stats";
const DB_VERSION = 1;
const STORE_NAME = "net_stats";

// Constant defining the maximum values allowed per interface. If more, older
// will be erased.
const VALUES_MAX_LENGTH = 6 * 30;

// Constant defining the rate of the samples. Daily.
const SAMPLE_RATE = 1000 * 60 * 60 * 24;

this.NetworkStatsDB = function NetworkStatsDB(aGlobal) {
  if (DEBUG) {
    debug("Constructor");
  }
  this.initDBHelper(DB_NAME, DB_VERSION, [STORE_NAME], aGlobal);
}

NetworkStatsDB.prototype = {
  __proto__: IndexedDBHelper.prototype,

  dbNewTxn: function dbNewTxn(txn_type, callback, txnCb) {
    function successCb(result) {
      txnCb(null, result);
    }
    function errorCb(error) {
      txnCb(error, null);
    }
    return this.newTxn(txn_type, STORE_NAME, callback, successCb, errorCb);
  },

  upgradeSchema: function upgradeSchema(aTransaction, aDb, aOldVersion, aNewVersion) {
    if (DEBUG) {
      debug("upgrade schema from: " + aOldVersion + " to " + aNewVersion + " called!");
    }
    let db = aDb;
    let objectStore;
    for (let currVersion = aOldVersion; currVersion < aNewVersion; currVersion++) {
      if (currVersion == 0) {
        /**
         * Create the initial database schema.
         */

        objectStore = db.createObjectStore(STORE_NAME, { keyPath: ["connectionType", "timestamp"] });
        objectStore.createIndex("connectionType", "connectionType", { unique: false });
        objectStore.createIndex("timestamp", "timestamp", { unique: false });
        objectStore.createIndex("rxBytes", "rxBytes", { unique: false });
        objectStore.createIndex("txBytes", "txBytes", { unique: false });
        objectStore.createIndex("rxTotalBytes", "rxTotalBytes", { unique: false });
        objectStore.createIndex("txTotalBytes", "txTotalBytes", { unique: false });
        if (DEBUG) {
          debug("Created object stores and indexes");
        }
      }
    }
  },

  convertDate: function convertDate(aDate) {
    // Convert to UTC according to timezone and
    // filter timestamp to get SAMPLE_RATE precission
    let timestamp = aDate.getTime() - aDate.getTimezoneOffset() * 60 * 1000;
    timestamp = Math.floor(timestamp / SAMPLE_RATE) * SAMPLE_RATE;
    return timestamp;
  },

  saveStats: function saveStats(stats, aResultCb) {
    let timestamp = this.convertDate(stats.date);

    stats = {connectionType: stats.connectionType,
             timestamp:      timestamp,
             rxBytes:        0,
             txBytes:        0,
             rxTotalBytes:   stats.rxBytes,
             txTotalBytes:   stats.txBytes};

    this.dbNewTxn("readwrite", function(txn, store) {
      if (DEBUG) {
        debug("Filtered time: " + new Date(timestamp));
        debug("New stats: " + JSON.stringify(stats));
      }

      let request = store.index("connectionType").openCursor(stats.connectionType, "prev");
      request.onsuccess = function onsuccess(event) {
        let cursor = event.target.result;
        if (!cursor) {
          // Empty, so save first element.
          this._saveStats(txn, store, stats);
          return;
        }

        // There are old samples
        if (DEBUG) {
          debug("Last value " + JSON.stringify(cursor.value));
        }

        // Remove stats previous to now - VALUE_MAX_LENGTH
        this._removeOldStats(txn, store, stats.connectionType, stats.timestamp);

        // Process stats before save
        this._processSamplesDiff(txn, store, cursor, stats);
      }.bind(this);
    }.bind(this), aResultCb);
  },

  /*
   * This function check that stats are saved in the database following the sample rate.
   * In this way is easier to find elements when stats are requested.
   */
  _processSamplesDiff: function _processSamplesDiff(txn, store, lastSampleCursor, newSample) {
    let lastSample = lastSampleCursor.value;

    // Get difference between last and new sample.
    let diff = (newSample.timestamp - lastSample.timestamp) / SAMPLE_RATE;
    if (diff % 1) {
      // diff is decimal, so some error happened because samples are stored as a multiple
      // of SAMPLE_RATE
      txn.abort();
      throw new Error("Error processing samples");
    }

    if (DEBUG) {
      debug("New: " + newSample.timestamp + " - Last: " + lastSample.timestamp + " - diff: " + diff);
    }

    let rxDiff = newSample.rxTotalBytes - lastSample.rxTotalBytes;
    let txDiff = newSample.txTotalBytes - lastSample.txTotalBytes;
    if (rxDiff < 0 || txDiff < 0) {
      rxDiff = newSample.rxTotalBytes;
      txDiff = newSample.txTotalBytes;
    }
    newSample.rxBytes = rxDiff;
    newSample.txBytes = txDiff;

    if (diff == 1) {
      // New element.
      this._saveStats(txn, store, newSample);
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
        let time = newSample.timestamp - SAMPLE_RATE * (i + 1);
        let sample = {connectionType: newSample.connectionType,
                      timestamp:      time,
                      rxBytes:        0,
                      txBytes:        0,
                      rxTotalBytes:   lastSample.rxTotalBytes,
                      txTotalBytes:   lastSample.txTotalBytes};
        data.push(sample);
      }

      data.push(newSample);
      this._saveStats(txn, store, data);
      return;
    }
    if (diff == 0 || diff < 0) {
      // New element received before samplerate period.
      // It means that device has been restarted (or clock / timezone change).
      // Update element.

      // If diff < 0, clock or timezone changed back. Place data in the last sample.

      lastSample.rxBytes += rxDiff;
      lastSample.txBytes += txDiff;
      lastSample.rxTotalBytes = newSample.rxTotalBytes;
      lastSample.txTotalBytes = newSample.txTotalBytes;
      if (DEBUG) {
        debug("Update: " + JSON.stringify(lastSample));
      }
      let req = lastSampleCursor.update(lastSample);
    }
  },

  _saveStats: function _saveStats(txn, store, networkStats) {
    if (DEBUG) {
      debug("_saveStats: " + JSON.stringify(networkStats));
    }

    if (Array.isArray(networkStats)) {
      let len = networkStats.length - 1;
      for (let i = 0; i <= len; i++) {
        store.put(networkStats[i]);
      }
    } else {
      store.put(networkStats);
    }
  },

  _removeOldStats: function _removeOldStats(txn, store, connType, date) {
    // Callback function to remove old items when new ones are added.
    let filterDate = date - (SAMPLE_RATE * VALUES_MAX_LENGTH - 1);
    let lowFilter = [connType, 0];
    let upFilter = [connType, filterDate];
    let range = this.dbGlobal.IDBKeyRange.bound(lowFilter, upFilter, false, false);
    store.openCursor(range).onsuccess = function(event) {
      var cursor = event.target.result;
      if (cursor) {
        cursor.delete();
        cursor.continue();
      }
    }.bind(this);
  },

  clear: function clear(aResultCb) {
    this.dbNewTxn("readwrite", function(txn, store) {
      if (DEBUG) {
        debug("Going to clear all!");
      }
      store.clear();
    }, aResultCb);
  },

  find: function find(aResultCb, aOptions) {
    let start = this.convertDate(aOptions.start);
    let end = this.convertDate(aOptions.end);

    if (DEBUG) {
      debug("Find: connectionType:" + aOptions.connectionType + " start: " + start + " end: " + end);
      debug("Start time: " + new Date(start));
      debug("End time: " + new Date(end));
    }

    this.dbNewTxn("readonly", function(txn, store) {
      let lowFilter = [aOptions.connectionType, start];
      let upFilter = [aOptions.connectionType, end];
      let range = this.dbGlobal.IDBKeyRange.bound(lowFilter, upFilter, false, false);

      let data = [];

      if (!txn.result) {
        txn.result = {};
      }

      let request = store.openCursor(range).onsuccess = function(event) {
        var cursor = event.target.result;
        if (cursor){
          data.push({ rxBytes: cursor.value.rxBytes,
                      txBytes: cursor.value.txBytes,
                      date: new Date(cursor.value.timestamp) });
          cursor.continue();
          return;
        }

        // When requested samples (start / end) are not in the range of now and
        // now - VALUES_MAX_LENGTH, fill with empty samples.
        this.fillResultSamples(start, end, data);

        txn.result.connectionType = aOptions.connectionType;
        txn.result.start = aOptions.start;
        txn.result.end = aOptions.end;
        txn.result.data = data;
      }.bind(this);
    }.bind(this), aResultCb);
  },

  findAll: function findAll(aResultCb, aOptions) {
    let start = this.convertDate(aOptions.start);
    let end = this.convertDate(aOptions.end);

    if (DEBUG) {
      debug("FindAll: start: " + start + " end: " + end + "\n");
    }

    let self = this;
    this.dbNewTxn("readonly", function(txn, store) {
      let lowFilter = start;
      let upFilter = end;
      let range = this.dbGlobal.IDBKeyRange.bound(lowFilter, upFilter, false, false);

      let data = [];

      if (!txn.result) {
        txn.result = {};
      }

      let request = store.index("timestamp").openCursor(range).onsuccess = function(event) {
        var cursor = event.target.result;
        if (cursor) {
          if (data.length > 0 && data[data.length - 1].date.getTime() == cursor.value.timestamp) {
            // Time is the same, so add values.
            data[data.length - 1].rxBytes += cursor.value.rxBytes;
            data[data.length - 1].txBytes += cursor.value.txBytes;
          } else {
            data.push({ rxBytes: cursor.value.rxBytes,
                        txBytes: cursor.value.txBytes,
                        date: new Date(cursor.value.timestamp) });
          }
          cursor.continue();
          return;
        }

        this.fillResultSamples(start, end, data);

        txn.result.connectionType = aOptions.connectionType;
        txn.result.start = aOptions.start;
        txn.result.end = aOptions.end;
        txn.result.data = data;
      }.bind(this);
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

  get sampleRate () {
    return SAMPLE_RATE;
  },

  get maxStorageSamples () {
    return VALUES_MAX_LENGTH;
  },

  logAllRecords: function logAllRecords(aResultCb) {
    this.dbNewTxn("readonly", function(txn, store) {
      store.mozGetAll().onsuccess = function onsuccess(event) {
        txn.result = event.target.result;
      };
    }, aResultCb);
  },
};
