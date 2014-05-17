/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'mmdb_head.js';

const DBNAME = "test_mmdb_upgradeSchema_current_structure:" + newUUID();

const LAYOUT = {
  sms: {
    keyPath: "id",
    autoIncrement: false,
    indice: {
      delivery: {
        keyPath: "deliveryIndex",
        multiEntry: false,
        unique: false,
      },

      envelopeId: {
        keyPath: "envelopeIdIndex",
        multiEntry: false,
        unique: true,
      },

      participantIds: {
        keyPath: "participantIdsIndex",
        multiEntry: true,
        unique: false,
      },

      read: {
        keyPath: "readIndex",
        multiEntry: false,
        unique: false,
      },

      threadId: {
        keyPath: "threadIdIndex",
        multiEntry: false,
        unique: false,
      },

      timestamp: {
        keyPath: "timestamp",
        multiEntry: false,
        unique: false,
      },

      transactionId: {
        keyPath: "transactionIdIndex",
        multiEntry: false,
        unique: true,
      }
    },
  },

  thread: {
    keyPath: "id",
    autoIncrement: true,
    indice: {
      lastTimestamp: {
        keyPath: "lastTimestamp",
        multiEntry: false,
        unique: false,
      },

      participantIds: {
        keyPath: "participantIds",
        multiEntry: false,
        unique: false,
      }
    },
  },

  participant: {
    keyPath: "id",
    autoIncrement: true,
    indice: {
      addresses: {
        keyPath: "addresses",
        multiEntry: true,
        unique: false,
      }
    },
  },

  "sms-segment": {
    keyPath: "id",
    autoIncrement: true,
    indice: {
      hash: {
        keyPath: "hash",
        multiEntry: false,
        unique: true,
      }
    },
  }
};

function verifyIndex(aIndex, aIndexLayout) {
  log("  Verifying index '" + aIndex.name + "'");

  is(aIndex.keyPath, aIndexLayout.keyPath, "aIndex.keyPath");
  is(aIndex.multiEntry, aIndexLayout.multiEntry, "aIndex.multiEntry");
  is(aIndex.unique, aIndexLayout.unique, "aIndex.unique");
}

function verifyStore(aObjectStore, aStoreLayout) {
  log("Verifying object store '" + aObjectStore.name + "'");

  is(aObjectStore.keyPath, aStoreLayout.keyPath, "aObjectStore.keyPath");
  is(aObjectStore.autoIncrement, aStoreLayout.autoIncrement,
     "aObjectStore.autoIncrement");

  let expectedIndexNames = Object.keys(aStoreLayout.indice);
  for (let i = 0; i < aObjectStore.indexNames.length; i++) {
    let indexName = aObjectStore.indexNames.item(i);

    let index = expectedIndexNames.indexOf(indexName);
    ok(index >= 0, "Index name '" + indexName + "' validity");
    expectedIndexNames.splice(index, 1);

    verifyIndex(aObjectStore.index(indexName), aStoreLayout.indice[indexName]);
  }

  // All index names should have been verified and leaves expectedIndexNames an
  // empty array.
  is(expectedIndexNames.length, 0, "Extra indice: " + expectedIndexNames);
}

function verifyDatabase(aMmdb) {
  let deferred = Promise.defer();

  let expectedStoreNames = Object.keys(LAYOUT);
  aMmdb.newTxn("readonly", function(aError, aTransaction, aObjectStores) {
    if (!Array.isArray(aObjectStores)) {
      // When we have only one object store open, aObjectStores is an instance
      // of IDBObjectStore.  Push it to an array for convenience here.
      aObjectStores = [aObjectStores];
    }

    is(aObjectStores.length, expectedStoreNames.length,
       "expected number of object stores");

    let slicedStoreNames = expectedStoreNames.slice();
    for (let i = 0; i < aObjectStores.length; i++) {
      let objectStore = aObjectStores[i];

      let index = slicedStoreNames.indexOf(objectStore.name);
      ok(index >= 0, "objectStore.name '" + objectStore.name + "' validity");
      slicedStoreNames.splice(index, 1);

      verifyStore(objectStore, LAYOUT[objectStore.name]);
    }

    // All store names should have been verified and leaves slicedStoreNames an
    // empty array.
    is(slicedStoreNames.length, 0, "Extra object stores: " + slicedStoreNames);

    deferred.resolve(aMmdb);
  }, expectedStoreNames);

  return deferred.promise;
}

startTestBase(function testCaseMain() {
  return initMobileMessageDB(newMobileMessageDB(), DBNAME, 0)
    .then(verifyDatabase)
    .then(closeMobileMessageDB);
});
