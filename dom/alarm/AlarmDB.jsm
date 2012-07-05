/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AlarmDB"];

/* static functions */
const DEBUG = false;

function debug(aStr) {
  if (DEBUG)
    dump("AlarmDB: " + aStr + "\n");
}

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/IndexedDBHelper.jsm");

const ALARMDB_NAME    = "alarms";
const ALARMDB_VERSION = 1;
const ALARMSTORE_NAME = "alarms";

function AlarmDB(aGlobal) {
  debug("AlarmDB()");
  this._global = aGlobal;
}

AlarmDB.prototype = {
  __proto__: IndexedDBHelper.prototype,

  init: function init(aGlobal) {
    debug("init()");

    this.initDBHelper(ALARMDB_NAME, ALARMDB_VERSION, ALARMSTORE_NAME, aGlobal);
  },

  upgradeSchema: function upgradeSchema(aTransaction, aDb, aOldVersion, aNewVersion) {
    debug("upgradeSchema()");

    let objectStore = aDb.createObjectStore(ALARMSTORE_NAME, { keyPath: "id", autoIncrement: true });

    objectStore.createIndex("date",           "date",           { unique: false });
    objectStore.createIndex("ignoreTimezone", "ignoreTimezone", { unique: false });
    objectStore.createIndex("timezoneOffset", "timezoneOffset", { unique: false });
    objectStore.createIndex("data",           "data",           { unique: false });
    objectStore.createIndex("manifestURL",    "manifestURL",    { unique: false });

    debug("Created object stores and indexes");
  },

  /**
   * @param aAlarm
   *        The record to be added.
   * @param aSuccessCb
   *        Callback function to invoke with result ID.
   * @param aErrorCb [optional]
   *        Callback function to invoke when there was an error.
   */
  add: function add(aAlarm, aSuccessCb, aErrorCb) {
    debug("add()");

    this.newTxn(
      "readwrite", 
      function txnCb(aTxn, aStore) {
        debug("Going to add " + JSON.stringify(aAlarm));
        aStore.put(aAlarm).onsuccess = function setTxnResult(aEvent) {
          aTxn.result = aEvent.target.result;
          debug("Request successful. New record ID: " + aTxn.result);
        };
      },
      aSuccessCb, 
      aErrorCb
    );
  },

  /**
   * @param aId
   *        The ID of record to be removed.
   * @param aSuccessCb
   *        Callback function to invoke with result.
   * @param aErrorCb [optional]
   *        Callback function to invoke when there was an error.
   */
  remove: function remove(aId, aSuccessCb, aErrorCb) {
    debug("remove()");

    this.newTxn(
      "readwrite", 
      function txnCb(aTxn, aStore) {
        debug("Going to remove " + aId);
        aStore.delete(aId);
      }, 
      aSuccessCb, 
      aErrorCb
    );
  },

  /**
   * @param aSuccessCb
   *        Callback function to invoke with result array.
   * @param aErrorCb [optional]
   *        Callback function to invoke when there was an error.
   */
  getAll: function getAll(aSuccessCb, aErrorCb) {
    debug("getAll()");

    this.newTxn(
      "readonly", 
      function txnCb(aTxn, aStore) {
        if (!aTxn.result)
          aTxn.result = {};

        aStore.mozGetAll().onsuccess = function setTxnResult(aEvent) {
          aTxn.result = aEvent.target.result;
          debug("Request successful. Record count: " + aTxn.result.length);
        };
      }, 
      aSuccessCb, 
      aErrorCb
    );
  }
};
