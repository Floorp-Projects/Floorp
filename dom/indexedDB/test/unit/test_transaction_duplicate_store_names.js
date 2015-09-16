
/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps() {
  const dbName = this.window ?
                 window.location.pathname :
                 "test_transaction_duplicate_store_names";
  const dbVersion = 1;
  const objectStoreName = "foo";
  const data = { };
  const dataKey = 1;

  let request = indexedDB.open(dbName, dbVersion);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;

  let event = yield undefined;

  let db = event.target.result;
  let objectStore = db.createObjectStore(objectStoreName);
  objectStore.add(data, dataKey);

  event = yield undefined;

  db = event.target.result;

  let transaction = db.transaction([objectStoreName, objectStoreName], "readwrite");
  transaction.onerror = errorHandler;
  transaction.oncomplete = grabEventAndContinueHandler;

  event = yield undefined;

  ok(true, "Transaction created successfully");

  finishTest();
  yield undefined;
}
