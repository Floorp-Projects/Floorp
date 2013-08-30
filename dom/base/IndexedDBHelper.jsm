/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let DEBUG = 0;
let debug;
if (DEBUG) {
  debug = function (s) { dump("-*- IndexedDBHelper: " + s + "\n"); }
} else {
  debug = function (s) {}
}

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = ["IndexedDBHelper"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

this.IndexedDBHelper = function IndexedDBHelper() {}

IndexedDBHelper.prototype = {

  // Cache the database
  _db: null,

  // Close the database
  close: function close() {
    if (this._db) {
      this._db.close();
      this._db = null;
    }
  },

  /**
   * Open a new database.
   * User has to provide upgradeSchema.
   *
   * @param successCb
   *        Success callback to call once database is open.
   * @param failureCb
   *        Error callback to call when an error is encountered.
   */
  open: function open(aSuccessCb, aFailureCb) {
    let self = this;
    if (DEBUG) debug("Try to open database:" + self.dbName + " " + self.dbVersion);
    let req = this.dbGlobal.indexedDB.open(this.dbName, this.dbVersion);
    req.onsuccess = function (event) {
      if (DEBUG) debug("Opened database:" + self.dbName + " " + self.dbVersion);
      self._db = event.target.result;
      self._db.onversionchange = function(event) {
        if (DEBUG) debug("WARNING: DB modified from a different window.");
      }
      aSuccessCb();
    };

    req.onupgradeneeded = function (aEvent) {
      if (DEBUG) {
        debug("Database needs upgrade:" + self.dbName + aEvent.oldVersion + aEvent.newVersion);
        debug("Correct new database version:" + (aEvent.newVersion == this.dbVersion));
      }

      let _db = aEvent.target.result;
      self.upgradeSchema(req.transaction, _db, aEvent.oldVersion, aEvent.newVersion);
    };
    req.onerror = function (aEvent) {
      if (DEBUG) debug("Failed to open database: " + self.dbName);
      aFailureCb(aEvent.target.error.name);
    };
    req.onblocked = function (aEvent) {
      if (DEBUG) debug("Opening database request is blocked.");
    };
  },

  /**
   * Use the cached DB or open a new one.
   *
   * @param successCb
   *        Success callback to call.
   * @param failureCb
   *        Error callback to call when an error is encountered.
   */
  ensureDB: function ensureDB(aSuccessCb, aFailureCb) {
    if (this._db) {
      if (DEBUG) debug("ensureDB: already have a database, returning early.");
      aSuccessCb();
      return;
    }
    this.open(aSuccessCb, aFailureCb);
  },

  /**
   * Start a new transaction.
   *
   * @param txn_type
   *        Type of transaction (e.g. "readwrite")
   * @param store_name
   *        The object store you want to be passed to the callback
   * @param callback
   *        Function to call when the transaction is available. It will
   *        be invoked with the transaction and the `store' object store.
   * @param successCb
   *        Success callback to call on a successful transaction commit.
   *        The result is stored in txn.result.
   * @param failureCb
   *        Error callback to call when an error is encountered.
   */
  newTxn: function newTxn(txn_type, store_name, callback, successCb, failureCb) {
    this.ensureDB(function () {
      if (DEBUG) debug("Starting new transaction" + txn_type);
      let txn = this._db.transaction(this.dbStoreNames, txn_type);
      if (DEBUG) debug("Retrieving object store", this.dbName);
      let store = txn.objectStore(store_name);

      txn.oncomplete = function (event) {
        if (DEBUG) debug("Transaction complete. Returning to callback.");
        if (successCb) {
          successCb(txn.result);
        }
      };

      txn.onabort = function (event) {
        if (DEBUG) debug("Caught error on transaction");
        /*
         * event.target.error may be null
         * if txn was aborted by calling txn.abort()
         */
        if (failureCb) {
          if (event.target.error) {
            failureCb(event.target.error.name);
          } else {
            failureCb("UnknownError");
          }
        }
      };
      callback(txn, store);
    }.bind(this), failureCb);
  },

  /**
   * Initialize the DB. Does not call open.
   *
   * @param aDBName
   *        DB name for the open call.
   * @param aDBVersion
   *        Current DB version. User has to implement upgradeSchema.
   * @param aDBStoreName
   *        ObjectStore that is used.
   * @param aGlobal
   *        Global object that has indexedDB property.
   */
  initDBHelper: function initDBHelper(aDBName, aDBVersion, aDBStoreNames, aGlobal) {
    this.dbName = aDBName;
    this.dbVersion = aDBVersion;
    this.dbStoreNames = aDBStoreNames;
    this.dbGlobal = aGlobal;
  }
}
