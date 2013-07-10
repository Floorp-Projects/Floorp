/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
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

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;
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

    let ex;
    try {
      objectStore.createIndex("Hola", ["foo"], { multiEntry: true });
    }
    catch(e) {
      ex = e;
    }
    ok(ex, "createIndex with array keyPath and multiEntry should throw");
    is(ex.name, "InvalidAccessError", "should throw right exception");
    ok(ex instanceof DOMException, "should throw right exception");
    is(ex.code, DOMException.INVALID_ACCESS_ERR, "should throw right exception");

    try {
      objectStore.createIndex("foo", "bar", 10);
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
      is(event.target.transaction.mode, "versionchange",
         "transaction has the correct mode");
      is(event.target.transaction.objectStoreNames.length, i + 1,
         "transaction only has one object store");
      ok(event.target.transaction.objectStoreNames.contains(objectStoreName),
         "transaction has the correct object store");
    }
  }

  request.onsuccess = grabEventAndContinueHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;

  event = yield undefined;

  finishTest();
  yield undefined;
}
