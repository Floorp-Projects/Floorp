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

Cu.import('resource://gre/modules/IndexedDBHelper.jsm');

this.DataStoreDB = function DataStoreDB() {}

DataStoreDB.prototype = {

  __proto__: IndexedDBHelper.prototype,

  upgradeSchema: function(aTransaction, aDb, aOldVersion, aNewVersion) {
    debug('updateSchema');
    aDb.createObjectStore(DATASTOREDB_OBJECTSTORE_NAME, { autoIncrement: true });
  },

  init: function(aOrigin, aName) {
    let dbName = aOrigin + '_' + aName;
    this.initDBHelper(dbName, DATASTOREDB_VERSION,
                      [DATASTOREDB_OBJECTSTORE_NAME]);
  },

  txn: function(aType, aCallback, aErrorCb) {
    debug('Transaction request');
    this.newTxn(
      aType,
      DATASTOREDB_OBJECTSTORE_NAME,
      aCallback,
      function() {},
      aErrorCb
    );
  },

  delete: function() {
    debug('delete');
    this.close();
    indexedDB.deleteDatabase(this.dbName);
    debug('database deleted');
  }
}
