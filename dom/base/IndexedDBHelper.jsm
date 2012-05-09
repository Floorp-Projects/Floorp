/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

let DEBUG = 0;
if (DEBUG) {
  debug = function (s) { dump("-*- IndexedDBHelper: " + s + "\n"); }
} else {
  debug = function (s) {}
}

const Cu = Components.utils; 
const Cc = Components.classes;
const Ci = Components.interfaces;

let EXPORTED_SYMBOLS = ["IndexedDBHelper"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function IndexedDBHelper() { }

IndexedDBHelper.prototype = {
  
  // Cache the database
  _db: null,

  // Close the database
  close: function close() {
    if (this._db) {
      this._db.close();
    }
  },

  /**
   * Open a new database.
   * User has to provide createSchema and upgradeSchema.
   * 
   * @param successCb
   *        Success callback to call once database is open.
   * @param failureCb
   *        Error callback to call when an error is encountered.
   */
  open: function open(aSuccessCb, aFailureCb) {
    let self = this;
    debug("Try to open database:" + this.dbName + " " + this.dbVersion);
    let req = this.dbGlobal.mozIndexedDB.open(this.dbName, this.dbVersion);
    req.onsuccess = function (event) {
      debug("Opened database:" + self.dbName + " " + self.dbName);
      self._db = event.target.result;
      self._db.onversionchange = function(event) {
        debug("WARNING: DB modified from a different window.");
      }
      aSuccessCb();
    };

    req.onupgradeneeded = function (aEvent) {
      debug("Database needs upgrade:" + this.dbName + aEvent.oldVersion + aEvent.newVersion);
      debug("Correct new database version:" + aEvent.newVersion == this.dbVersion);

      let _db = aEvent.target.result;
      switch (aEvent.oldVersion) {
        case 0:
          debug("New database");
          self.createSchema(_db);
          break;
        default:
          self.upgradeSchema(_db, aEvent.oldVersion, aEvent.newVersion);
          break;
      }
    };
    req.onerror = function (aEvent) {
      debug("Failed to open database:" + this.dbName);
      aFailureCb(aEvent.target.errorMessage);
    };
    req.onblocked = function (aEvent) {
      debug("Opening database request is blocked.");
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
      debug("ensureDB: already have a database, returning early.");
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
   * @param callback
   *        Function to call when the transaction is available. It will
   *        be invoked with the transaction and the 'aDBStoreName' object store.
   * @param successCb
   *        Success callback to call on a successful transaction commit.
   *        The result is stored in txn.result.
   * @param failureCb
   *        Error callback to call when an error is encountered.
   */
  newTxn: function newTxn(txn_type, callback, successCb, failureCb) {
    this.ensureDB(function () {
      debug("Starting new transaction" + txn_type);
      let txn = this._db.transaction(this.dbName, txn_type);
      debug("Retrieving object store", this.dbName);
      let store = txn.objectStore(this.dbStoreName);

      txn.oncomplete = function (event) {
        debug("Transaction complete. Returning to callback.");
        successCb(txn.result);
      };

      txn.onabort = function (event) {
        debug("Caught error on transaction");
        // FIXXMEE: this will work in the future. Bug 748630
        // failureCb(event.target.error.name);
        failureCb("UnknownError");
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
   *        Current DB version. User has to implement createSchema and upgradeSchema.
   * @param aDBStoreName
   *        ObjectStore that is used.
   * @param aGlobal
   *        Global object that has mozIndexedDB property.
   */
  initDBHelper: function initDBHelper(aDBName, aDBVersion, aDBStoreName, aGlobal) {
    this.dbName = aDBName;
    this.dbVersion = aDBVersion;
    this.dbStoreName = aDBStoreName;
    this.dbGlobal = aGlobal;
  }
}
