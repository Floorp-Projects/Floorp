/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Don't modify this, instead set dom.push.debug.
var gDebuggingEnabled = false;

function debug(s) {
  if (gDebuggingEnabled) {
    dump("-*- PushDB.jsm: " + s + "\n");
  }
}

const Cu = Components.utils;
Cu.import("resource://gre/modules/IndexedDBHelper.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.importGlobalProperties(["indexedDB"]);

const prefs = new Preferences("dom.push.");

this.EXPORTED_SYMBOLS = ["PushDB"];

this.PushDB = function PushDB(dbName, dbVersion, dbStoreName, keyPath, model) {
  debug("PushDB()");
  this._dbStoreName = dbStoreName;
  this._keyPath = keyPath;
  this._model = model;

  // set the indexeddb database
  this.initDBHelper(dbName, dbVersion,
                    [dbStoreName]);
  gDebuggingEnabled = prefs.get("debug");
  prefs.observe("debug", this);
};

this.PushDB.prototype = {
  __proto__: IndexedDBHelper.prototype,

  toPushRecord: function(record) {
    if (!record) {
      return;
    }
    return new this._model(record);
  },

  isValidRecord: function(record) {
    return record && typeof record.scope == "string" &&
           typeof record.originAttributes == "string" &&
           record.quota >= 0 &&
           typeof record[this._keyPath] == "string";
  },

  upgradeSchema: function(aTransaction, aDb, aOldVersion, aNewVersion) {
    if (aOldVersion <= 3) {
      //XXXnsm We haven't shipped Push during this upgrade, so I'm just going to throw old
      //registrations away without even informing the app.
      if (aDb.objectStoreNames.contains(this._dbStoreName)) {
        aDb.deleteObjectStore(this._dbStoreName);
      }

      let objectStore = aDb.createObjectStore(this._dbStoreName,
                                              { keyPath: this._keyPath });

      // index to fetch records based on endpoints. used by unregister
      objectStore.createIndex("pushEndpoint", "pushEndpoint", { unique: true });

      // index to fetch records by identifiers.
      // In the current security model, the originAttributes distinguish between
      // different 'apps' on the same origin. Since ServiceWorkers are
      // same-origin to the scope they are registered for, the attributes and
      // scope are enough to reconstruct a valid principal.
      objectStore.createIndex("identifiers", ["scope", "originAttributes"], { unique: true });
      objectStore.createIndex("originAttributes", "originAttributes", { unique: false });
    }

    if (aOldVersion < 4) {
      let objectStore = aTransaction.objectStore(this._dbStoreName);

      // index to fetch active and expired registrations.
      objectStore.createIndex("quota", "quota", { unique: false });
    }
  },

  /*
   * @param aRecord
   *        The record to be added.
   */

  put: function(aRecord) {
    debug("put()" + JSON.stringify(aRecord));
    if (!this.isValidRecord(aRecord)) {
      return Promise.reject(new TypeError(
        "Scope, originAttributes, and quota are required! " +
          JSON.stringify(aRecord)
        )
      );
    }

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readwrite",
        this._dbStoreName,
        (aTxn, aStore) => {
          aTxn.result = undefined;

          aStore.put(aRecord).onsuccess = aEvent => {
            debug("Request successful. Updated record ID: " +
                  aEvent.target.result);
            aTxn.result = this.toPushRecord(aRecord);
          };
        },
        resolve,
        reject
      )
    );
  },

  /*
   * @param aKeyID
   *        The ID of record to be deleted.
   */
  delete: function(aKeyID) {
    debug("delete()");

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readwrite",
        this._dbStoreName,
        function txnCb(aTxn, aStore) {
          debug("Going to delete " + aKeyID);
          aStore.delete(aKeyID);
        },
        resolve,
        reject
      )
    );
  },

  clearAll: function clear() {
    return new Promise((resolve, reject) =>
      this.newTxn(
        "readwrite",
        this._dbStoreName,
        function (aTxn, aStore) {
          debug("Going to clear all!");
          aStore.clear();
        },
        resolve,
        reject
      )
    );
  },

  // testFn(record) is called with a database record and should return true if
  // that record should be deleted.
  clearIf: function(testFn) {
    debug("clearIf()");
    return new Promise((resolve, reject) =>
      this.newTxn(
        "readwrite",
        this._dbStoreName,
        (aTxn, aStore) => {
          aTxn.result = undefined;

          aStore.openCursor().onsuccess = event => {
            let cursor = event.target.result;
            if (cursor) {
              if (testFn(this.toPushRecord(cursor.value))) {
                let deleteRequest = cursor.delete();
                deleteRequest.onerror = e => {
                  debug("Failed to delete entry even when test succeeded!");
                }
              }
              cursor.continue();
            }
          }
        },
        resolve,
        reject
      )
    );
  },

  getByPushEndpoint: function(aPushEndpoint) {
    debug("getByPushEndpoint()");

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readonly",
        this._dbStoreName,
        (aTxn, aStore) => {
          aTxn.result = undefined;

          let index = aStore.index("pushEndpoint");
          index.get(aPushEndpoint).onsuccess = aEvent => {
            aTxn.result = this.toPushRecord(aEvent.target.result);
            debug("Fetch successful " + aEvent.target.result);
          };
        },
        resolve,
        reject
      )
    );
  },

  getByKeyID: function(aKeyID) {
    debug("getByKeyID()");

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readonly",
        this._dbStoreName,
        (aTxn, aStore) => {
          aTxn.result = undefined;

          aStore.get(aKeyID).onsuccess = aEvent => {
            aTxn.result = this.toPushRecord(aEvent.target.result);
            debug("Fetch successful " + aEvent.target.result);
          };
        },
        resolve,
        reject
      )
    );
  },

  getAllByOrigin: function(origin, originAttributes) {
    debug("getAllByOrigin()");

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readonly",
        this._dbStoreName,
        (aTxn, aStore) => {
          aTxn.result = [];

          let index = aStore.index("identifiers");
          let range = IDBKeyRange.bound(
            [origin, originAttributes],
            [origin + "\x7f", originAttributes]
          );
          index.openCursor(range).onsuccess = event => {
            let cursor = event.target.result;
            if (cursor) {
              aTxn.result.push(this.toPushRecord(cursor.value));
              cursor.continue();
            }
          };
        },
        resolve,
        reject
      )
    );
  },

  // Perform a unique match against { scope, originAttributes }
  getByIdentifiers: function(aPageRecord) {
    debug("getByIdentifiers() { " + aPageRecord.scope + ", " +
          JSON.stringify(aPageRecord.originAttributes) + " }");
    if (!aPageRecord.scope || aPageRecord.originAttributes == undefined) {
      return Promise.reject(
               new TypeError("Scope and originAttributes are required! " +
                             JSON.stringify(aPageRecord)));
    }

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readonly",
        this._dbStoreName,
        (aTxn, aStore) => {
          aTxn.result = undefined;

          let index = aStore.index("identifiers");
          let request = index.get(IDBKeyRange.only([aPageRecord.scope, aPageRecord.originAttributes]));
          request.onsuccess = aEvent => {
            aTxn.result = this.toPushRecord(aEvent.target.result);
          };
        },
        resolve,
        reject
      )
    );
  },

  _getAllByKey: function(aKeyName, aKeyValue) {
    return new Promise((resolve, reject) =>
      this.newTxn(
        "readonly",
        this._dbStoreName,
        (aTxn, aStore) => {
          aTxn.result = undefined;

          let index = aStore.index(aKeyName);
          // It seems ok to use getAll here, since unlike contacts or other
          // high storage APIs, we don't expect more than a handful of
          // registrations per domain, and usually only one.
          let getAllReq = index.mozGetAll(aKeyValue);
          getAllReq.onsuccess = aEvent => {
            aTxn.result = aEvent.target.result.map(
              record => this.toPushRecord(record));
          };
        },
        resolve,
        reject
      )
    );
  },

  // aOriginAttributes must be a string!
  getAllByOriginAttributes: function(aOriginAttributes) {
    if (typeof aOriginAttributes !== "string") {
      return Promise.reject("Expected string!");
    }
    return this._getAllByKey("originAttributes", aOriginAttributes);
  },

  getAllKeyIDs: function() {
    debug("getAllKeyIDs()");

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readonly",
        this._dbStoreName,
        (aTxn, aStore) => {
          aTxn.result = undefined;
          aStore.mozGetAll().onsuccess = event => {
            aTxn.result = event.target.result.map(
              record => this.toPushRecord(record));
          };
        },
        resolve,
        reject
      )
    );
  },

  _getAllByPushQuota: function(range) {
    debug("getAllByPushQuota()");

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readonly",
        this._dbStoreName,
        (aTxn, aStore) => {
          aTxn.result = [];

          let index = aStore.index("quota");
          index.openCursor(range).onsuccess = event => {
            let cursor = event.target.result;
            if (cursor) {
              aTxn.result.push(this.toPushRecord(cursor.value));
              cursor.continue();
            }
          };
        },
        resolve,
        reject
      )
    );
  },

  getAllUnexpired: function() {
    debug("getAllUnexpired()");
    return this._getAllByPushQuota(IDBKeyRange.lowerBound(1));
  },

  getAllExpired: function() {
    debug("getAllExpired()");
    return this._getAllByPushQuota(IDBKeyRange.only(0));
  },

  /**
   * Updates an existing push registration.
   *
   * @param {String} aKeyID The registration ID.
   * @param {Function} aUpdateFunc A function that receives the existing
   *  registration record as its argument, and returns a new record. If the
   *  function returns `null` or `undefined`, the record will not be updated.
   *  If the record does not exist, the function will not be called.
   * @returns {Promise} A promise resolved with either the updated record, or
   *  `undefined` if the record was not updated.
   */
  update: function(aKeyID, aUpdateFunc) {
    return new Promise((resolve, reject) =>
      this.newTxn(
        "readwrite",
        this._dbStoreName,
        (aTxn, aStore) => {
          aStore.get(aKeyID).onsuccess = aEvent => {
            aTxn.result = undefined;

            let record = aEvent.target.result;
            if (!record) {
              debug("update: Key ID " + aKeyID + " does not exist");
              return;
            }
            let newRecord = aUpdateFunc(this.toPushRecord(record));
            if (!this.isValidRecord(newRecord)) {
              debug("update: Ignoring invalid update for key ID " + aKeyID);
              return;
            }
            aStore.put(newRecord).onsuccess = aEvent => {
              debug("update: Update successful for key ID " + aKeyID);
              aTxn.result = newRecord;
            };
          };
        },
        resolve,
        reject
      )
    );
  },

  drop: function() {
    debug("drop()");

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readwrite",
        this._dbStoreName,
        function txnCb(aTxn, aStore) {
          aStore.clear();
        },
        resolve,
        reject
      )
    );
  },

  observe: function observe(aSubject, aTopic, aData) {
    if ((aTopic == "nsPref:changed") && (aData == "dom.push.debug"))
      gDebuggingEnabled = prefs.get("debug");
  }
};
