/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Don't modify this, instead set dom.push.debug.
let gDebuggingEnabled = false;

function debug(s) {
  if (gDebuggingEnabled) {
    dump("-*- PushDB.jsm: " + s + "\n");
  }
}

const Cu = Components.utils;
Cu.import("resource://gre/modules/IndexedDBHelper.jsm");
Cu.importGlobalProperties(["indexedDB"]);

this.EXPORTED_SYMBOLS = ["PushDB"];

this.PushDB = function PushDB(dbName, dbVersion, dbStoreName, schemaFunction) {
  debug("PushDB()");
  this._dbStoreName = dbStoreName;
  this._schemaFunction = schemaFunction;

  // set the indexeddb database
  this.initDBHelper(dbName, dbVersion,
                    [dbStoreName]);
};

this.PushDB.prototype = {
  __proto__: IndexedDBHelper.prototype,

  upgradeSchema: function(aTransaction, aDb, aOldVersion, aNewVersion) {
    if (this._schemaFunction) {
      this._schemaFunction(aTransaction, aDb, aOldVersion, aNewVersion, this);
    }
  },

  /*
   * @param aChannelRecord
   *        The record to be added.
   */

  put: function(aChannelRecord) {
    debug("put()" + JSON.stringify(aChannelRecord));

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readwrite",
        this._dbStoreName,
        function txnCb(aTxn, aStore) {
          debug("Going to put " + aChannelRecord.channelID);
          aStore.put(aChannelRecord).onsuccess = function setTxnResult(aEvent) {
            debug("Request successful. Updated record ID: " +
                  aEvent.target.result);
          };
        },
        resolve,
        reject
      )
    );
  },

  /*
   * @param aChannelID
   *        The ID of record to be deleted.
   */
  delete: function(aChannelID) {
    debug("delete()");

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readwrite",
        this._dbStoreName,
        function txnCb(aTxn, aStore) {
          debug("Going to delete " + aChannelID);
          aStore.delete(aChannelID);
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

  getByPushEndpoint: function(aPushEndpoint) {
    debug("getByPushEndpoint()");

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readonly",
        this._dbStoreName,
        function txnCb(aTxn, aStore) {
          aTxn.result = undefined;

          let index = aStore.index("pushEndpoint");
          index.get(aPushEndpoint).onsuccess = function setTxnResult(aEvent) {
            aTxn.result = aEvent.target.result;
            debug("Fetch successful " + aEvent.target.result);
          }
        },
        resolve,
        reject
      )
    );
  },

  getByChannelID: function(aChannelID) {
    debug("getByChannelID()");

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readonly",
        this._dbStoreName,
        function txnCb(aTxn, aStore) {
          aTxn.result = undefined;

          aStore.get(aChannelID).onsuccess = function setTxnResult(aEvent) {
            aTxn.result = aEvent.target.result;
            debug("Fetch successful " + aEvent.target.result);
          }
        },
        resolve,
        reject
      )
    );
  },


  getByScope: function(aScope) {
    debug("getByScope() " + aScope);

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readonly",
        this._dbStoreName,
        function txnCb(aTxn, aStore) {
          aTxn.result = undefined;

          let index = aStore.index("scope");
          index.get(aScope).onsuccess = function setTxnResult(aEvent) {
            aTxn.result = aEvent.target.result;
            debug("Fetch successful " + aEvent.target.result);
          }
        },
        resolve,
        reject
      )
    );
  },

  getAllChannelIDs: function() {
    debug("getAllChannelIDs()");

    return new Promise((resolve, reject) =>
      this.newTxn(
        "readonly",
        this._dbStoreName,
        function txnCb(aTxn, aStore) {
          aStore.mozGetAll().onsuccess = function(event) {
            aTxn.result = event.target.result;
          }
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

  observeDebug: function(enabled) {
    gDebuggingEnabled = enabled;
  }
};
