/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const objectStoreInfo = [
    { name: "1", options: { keyPath: null } },
    { name: "2", options: { keyPath: null, autoIncrement: true } },
    { name: "3", options: { keyPath: null, autoIncrement: false } },
    { name: "4", options: { keyPath: null } },
    { name: "5", options: { keyPath: "foo" } },
    { name: "6" },
    { name: "7", options: null },
    { name: "8", options: { autoIncrement: true } },
    { name: "9", options: { autoIncrement: false } },
    { name: "10", options: { keyPath: "foo", autoIncrement: false } },
    { name: "11", options: { keyPath: "foo", autoIncrement: true } },
    { name: "" },
    { name: null },
    { name: undefined }
  ];

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;

  let db = event.target.result;

  let count = db.objectStoreNames.length;
  is(count, 0, "correct objectStoreNames length");

  try {
    db.createObjectStore("foo", "bar");
    ok(false, "createObjectStore with bad options should throw");
  }
  catch(e) {
    ok(true, "createObjectStore with bad options");
  }

  ok(db.createObjectStore("foo", { foo: "" }),
     "createObjectStore with unknown options should not throw");
  db.deleteObjectStore("foo");

  for (let index in objectStoreInfo) {
    index = parseInt(index);
    const info = objectStoreInfo[index];

    let objectStore = info.hasOwnProperty("options") ?
                      db.createObjectStore(info.name, info.options) :
                      db.createObjectStore(info.name);

    is(db.objectStoreNames.length, index + 1,
       "updated objectStoreNames list");

    let name = info.name;
    if (name === null) {
      name = "null";
    }
    else if (name === undefined) {
      name = "undefined";
    }

    let found = false;
    for (let i = 0; i <= index; i++) {
      if (db.objectStoreNames.item(i) == name) {
        found = true;
        break;
      }
    }
    is(found, true, "objectStoreNames contains name");

    is(objectStore.name, name, "Bad name");
    is(objectStore.keyPath, info.options && info.options.keyPath ?
                            info.options.keyPath : null,
       "Bad keyPath");
    if(objectStore.indexNames.length, 0, "Bad indexNames");

    ok(event.target.transaction, "event has a transaction");
    ok(event.target.transaction.db === db, "transaction has the right db");
    is(event.target.transaction.mode, "versionchange",
       "transaction has the correct mode");
    is(event.target.transaction.objectStoreNames.length, index + 1,
       "transaction has correct objectStoreNames list");
    found = false;
    for (let j = 0; j < event.target.transaction.objectStoreNames.length;
         j++) {
      if (event.target.transaction.objectStoreNames.item(j) == name) {
        found = true;
        break;
      }
    }
    is(found, true, "transaction has correct objectStoreNames list");
  }

  // Can't handle autoincrement and empty keypath
  let ex;
  try {
    db.createObjectStore("storefail", { keyPath: "", autoIncrement: true });
  }
  catch(e) {
    ex = e;
  }
  ok(ex, "createObjectStore with empty keyPath and autoIncrement should throw");
  is(ex.name, "InvalidAccessError", "should throw right exception");
  ok(ex instanceof DOMException, "should throw right exception");
  is(ex.code, DOMException.INVALID_ACCESS_ERR, "should throw right exception");

  // Can't handle autoincrement and array keypath
  try {
    db.createObjectStore("storefail", { keyPath: ["a"], autoIncrement: true });
  }
  catch(e) {
    ex = e;
  }
  ok(ex, "createObjectStore with array keyPath and autoIncrement should throw");
  is(ex.name, "InvalidAccessError", "should throw right exception");
  ok(ex instanceof DOMException, "should throw right exception");
  is(ex.code, DOMException.INVALID_ACCESS_ERR, "should throw right exception");

  request.onsuccess = grabEventAndContinueHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;

  event = yield undefined;

  finishTest();
}
