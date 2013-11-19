/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ['DataStoreDB'];

function debug(s) {
  // dump('DEBUG DataStoreDB: ' + s + '\n');
}

const DATASTOREDB_VERSION = 1;
const DATASTOREDB_OBJECTSTORE_NAME = 'DataStoreDB';
const DATASTOREDB_REVISION = 'revision';
const DATASTOREDB_REVISION_INDEX = 'revisionIndex';

Cu.import('resource://gre/modules/IndexedDBHelper.jsm');
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.importGlobalProperties(["indexedDB"]);

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

this.DataStoreDB = function DataStoreDB() {}

DataStoreDB.prototype = {

  __proto__: IndexedDBHelper.prototype,

  upgradeSchema: function(aTransaction, aDb, aOldVersion, aNewVersion) {
    debug('updateSchema');
    aDb.createObjectStore(DATASTOREDB_OBJECTSTORE_NAME, { autoIncrement: true });
    let store = aDb.createObjectStore(DATASTOREDB_REVISION,
                                      { autoIncrement: true,
                                        keyPath: 'internalRevisionId' });
    store.createIndex(DATASTOREDB_REVISION_INDEX, 'revisionId', { unique: true });
  },

  init: function(aOwner, aName) {
    let dbName = aName + '|' + aOwner;
    this.initDBHelper(dbName, DATASTOREDB_VERSION,
                      [DATASTOREDB_OBJECTSTORE_NAME, DATASTOREDB_REVISION]);
  },

  txn: function(aType, aCallback, aErrorCb) {
    debug('Transaction request');
    this.newTxn(
      aType,
      aType == 'readonly'
        ? [ DATASTOREDB_OBJECTSTORE_NAME ] : [ DATASTOREDB_OBJECTSTORE_NAME, DATASTOREDB_REVISION ],
      function(aTxn, aStores) {
        aType == 'readonly' ? aCallback(aTxn, aStores[0], null) : aCallback(aTxn, aStores[0], aStores[1]);
      },
      function() {},
      aErrorCb
    );
  },

  cursorTxn: function(aCallback, aErrorCb) {
    debug('Cursor transaction request');
    this.newTxn(
      'readonly',
       [ DATASTOREDB_OBJECTSTORE_NAME, DATASTOREDB_REVISION ],
      function(aTxn, aStores) {
        aCallback(aTxn, aStores[0], aStores[1]);
      },
      function() {},
      aErrorCb
    );
  },

  revisionTxn: function(aType, aCallback, aErrorCb) {
    debug("Transaction request");
    this.newTxn(
      aType,
      DATASTOREDB_REVISION,
      aCallback,
      function() {},
      aErrorCb
    );
  },

  addRevision: function(aStore, aKey, aType, aSuccessCb) {
    debug("AddRevision: " + aKey + " - " + aType);
    let revisionId =  uuidgen.generateUUID().toString();
    let request = aStore.put({ revisionId: revisionId, objectId: aKey, operation: aType });
    request.onsuccess = function() {
      aSuccessCb(revisionId);
    }
  },

  getInternalRevisionId: function(aRevisionId, aStore, aSuccessCb) {
    debug('GetInternalRevisionId');
    let request = aStore.index(DATASTOREDB_REVISION_INDEX).getKey(aRevisionId);
    request.onsuccess = function(aEvent) {
      aSuccessCb(aEvent.target.result);
    }
  },

  clearRevisions: function(aStore, aSuccessCb) {
    debug("ClearRevisions");
    let request = aStore.clear();
    request.onsuccess = function() {
      aSuccessCb();
    }
  },

  delete: function() {
    debug('delete');
    this.close();
    indexedDB.deleteDatabase(this.dbName);
    debug('database deleted');
  }
}
