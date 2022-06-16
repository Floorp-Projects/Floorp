/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var DEBUG = 0;
var debug;
if (DEBUG) {
  debug = function(s) {
    dump("-*- IndexedDBHelper: " + s + "\n");
  };
} else {
  debug = function(s) {};
}

var EXPORTED_SYMBOLS = ["IndexedDBHelper"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function getErrorName(err) {
  return (err && err.name) || "UnknownError";
}

function IndexedDBHelper() {}

IndexedDBHelper.prototype = {
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
  open: function open(aCallback) {
    if (aCallback && !this._waitForOpenCallbacks.has(aCallback)) {
      this._waitForOpenCallbacks.add(aCallback);
      if (this._waitForOpenCallbacks.size !== 1) {
        return;
      }
    }

    let self = this;
    let invokeCallbacks = err => {
      for (let callback of self._waitForOpenCallbacks) {
        callback(err);
      }
      self._waitForOpenCallbacks.clear();
    };

    if (DEBUG) {
      debug("Try to open database:" + self.dbName + " " + self.dbVersion);
    }
    let req;
    try {
      req = indexedDB.open(this.dbName, this.dbVersion);
    } catch (e) {
      if (DEBUG) {
        debug("Error opening database: " + self.dbName);
      }
      Services.tm.dispatchToMainThread(() => invokeCallbacks(getErrorName(e)));
      return;
    }
    req.onsuccess = function(event) {
      if (DEBUG) {
        debug("Opened database:" + self.dbName + " " + self.dbVersion);
      }
      self._db = event.target.result;
      self._db.onversionchange = function(event) {
        if (DEBUG) {
          debug("WARNING: DB modified from a different window.");
        }
      };
      invokeCallbacks();
    };

    req.onupgradeneeded = function(aEvent) {
      if (DEBUG) {
        debug(
          "Database needs upgrade:" +
            self.dbName +
            aEvent.oldVersion +
            aEvent.newVersion
        );
        debug(
          "Correct new database version:" +
            (aEvent.newVersion == this.dbVersion)
        );
      }

      let _db = aEvent.target.result;
      self.upgradeSchema(
        req.transaction,
        _db,
        aEvent.oldVersion,
        aEvent.newVersion
      );
    };
    req.onerror = function(aEvent) {
      if (DEBUG) {
        debug("Failed to open database: " + self.dbName);
      }
      invokeCallbacks(getErrorName(aEvent.target.error));
    };
    req.onblocked = function(aEvent) {
      if (DEBUG) {
        debug("Opening database request is blocked.");
      }
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
      if (DEBUG) {
        debug("ensureDB: already have a database, returning early.");
      }
      if (aSuccessCb) {
        Services.tm.dispatchToMainThread(aSuccessCb);
      }
      return;
    }
    this.open(aError => {
      if (aError) {
        aFailureCb && aFailureCb(aError);
      } else {
        aSuccessCb && aSuccessCb();
      }
    });
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
   *        The result is stored in txn.result (in the callback function).
   * @param failureCb
   *        Error callback to call when an error is encountered.
   */
  newTxn: function newTxn(
    txn_type,
    store_name,
    callback,
    successCb,
    failureCb
  ) {
    this.ensureDB(() => {
      if (DEBUG) {
        debug("Starting new transaction" + txn_type);
      }
      let txn;
      try {
        txn = this._db.transaction(
          Array.isArray(store_name) ? store_name : this.dbStoreNames,
          txn_type
        );
      } catch (e) {
        if (DEBUG) {
          debug("Error starting transaction: " + this.dbName);
        }
        failureCb(getErrorName(e));
        return;
      }
      if (DEBUG) {
        debug("Retrieving object store: " + this.dbName);
      }
      let stores;
      if (Array.isArray(store_name)) {
        stores = [];
        for (let i = 0; i < store_name.length; ++i) {
          stores.push(txn.objectStore(store_name[i]));
        }
      } else {
        stores = txn.objectStore(store_name);
      }

      txn.oncomplete = function() {
        if (DEBUG) {
          debug("Transaction complete. Returning to callback.");
        }
        /*
         * txn.result property is not part of the transaction object returned
         * by this._db.transaction method called above.
         * The property is expected to be set in the callback function.
         * However, it can happen that the property is not set for some reason,
         * so we have to check if the property exists before calling the
         * success callback.
         */
        if (successCb) {
          if ("result" in txn) {
            successCb(txn.result);
          } else {
            successCb();
          }
        }
      };

      txn.onabort = function() {
        if (DEBUG) {
          debug("Caught error on transaction");
        }
        /*
         * txn.error property is part of the transaction object returned by
         * this._db.transaction method called above.
         * The attribute is defined in IDBTranscation WebIDL interface.
         * It may be null.
         */
        if (failureCb) {
          failureCb(getErrorName(txn.error));
        }
      };
      callback(txn, stores);
    }, failureCb);
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
   */
  initDBHelper: function initDBHelper(aDBName, aDBVersion, aDBStoreNames) {
    this.dbName = aDBName;
    this.dbVersion = aDBVersion;
    this.dbStoreNames = aDBStoreNames;
    // Cache the database.
    this._db = null;
    this._waitForOpenCallbacks = new Set();
  },
};
