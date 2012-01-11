/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const nsIIDBObjectStore = Components.interfaces.nsIIDBObjectStore;
  const nsIIDBTransaction = Components.interfaces.nsIIDBTransaction;

  const name = this.window ? window.location.pathname : "Splendid Test";
  const description = "My Test Database";
  const objectStoreInfo = [
    { name: "a", options: { keyPath: "id", autoIncrement: true } },
    { name: "b", options: { keyPath: "id", autoIncrement: false } },
  ];
  const indexInfo = [
    { name: "1", keyPath: "unique_value", options: { unique: true } },
    { name: "2", keyPath: "value", options: { unique: false } },
    { name: "3", keyPath: "value", options: { unique: false } },
    { name: "", keyPath: "value", options: { unique: false } },
    { name: null, keyPath: "value", options: { unique: false } },
    { name: undefined, keyPath: "value", options: { unique: false } },
  ];

  let request = mozIndexedDB.open(name, 1, description);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield;
  let db = event.target.result;

  for (let i = 0; i < objectStoreInfo.length; i++) {
    let info = objectStoreInfo[i];
    let objectStore = info.hasOwnProperty("options") ?
                      db.createObjectStore(info.name, info.options) :
                      db.createObjectStore(info.name);

    try {
      request = objectStore.createIndex("Hola");
      ok(false, "createIndex with no keyPath should throw");
    }
    catch(e) {
      ok(true, "createIndex with no keyPath should throw");
    }

    try {
      request = objectStore.createIndex("Hola", ["foo"], { multiEntry: true });
      ok(false, "createIndex with array keyPath and multiEntry should throw");
    }
    catch(e) {
      ok(true, "createIndex with array keyPath and multiEntry should throw");
    }

    try {
      request = objectStore.createIndex("Hola", []);
      ok(false, "createIndex with empty array keyPath should throw");
    }
    catch(e) {
      ok(true, "createIndex with empty array keyPath should throw");
    }

    try {
      request = objectStore.createIndex("foo", "bar", 10);
      ok(false, "createIndex with bad options should throw");
    }
    catch(e) {
      ok(true, "createIndex with bad options threw");
    }

    ok(objectStore.createIndex("foo", "bar", { foo: "" }),
       "createIndex with unknown options should not throw");
    objectStore.deleteIndex("foo");

    // Test index creation, and that it ends up in indexNames.
    let objectStoreName = info.name;
    for (let j = 0; j < indexInfo.length; j++) {
      let info = indexInfo[j];
      let count = objectStore.indexNames.length;
      let index = info.hasOwnProperty("options") ?
                  objectStore.createIndex(info.name, info.keyPath,
                                          info.options) :
                  objectStore.createIndex(info.name, info.keyPath);

      let name = info.name;
      if (name === null) {
        name = "null";
      }
      else if (name === undefined) {
        name = "undefined";
      }

      is(index.name, name, "correct name");
      is(index.keyPath, info.keyPath, "correct keyPath");
      is(index.unique, info.options.unique, "correct uniqueness");

      is(objectStore.indexNames.length, count + 1,
         "indexNames grew in size");
      let found = false;
      for (let k = 0; k < objectStore.indexNames.length; k++) {
        if (objectStore.indexNames.item(k) == name) {
          found = true;
          break;
        }
      }
      ok(found, "Name is on objectStore.indexNames");

      ok(event.target.transaction, "event has a transaction");
      ok(event.target.transaction.db === db,
         "transaction has the right db");
      is(event.target.transaction.readyState, nsIIDBTransaction.LOADING,
         "transaction has the correct readyState");
      is(event.target.transaction.mode, nsIIDBTransaction.VERSION_CHANGE,
         "transaction has the correct mode");
      is(event.target.transaction.objectStoreNames.length, i + 1,
         "transaction only has one object store");
      is(event.target.transaction.objectStoreNames.item(0), objectStoreName,
         "transaction has the correct object store");
    }
  }

  finishTest();
  yield;
}
