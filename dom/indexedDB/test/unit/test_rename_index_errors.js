/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const storeName = "test store";
  const indexName1 = "test index 1";
  const indexName2 = "test index 2";

  info("Setup test indexes.");
  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;

  let db = event.target.result;
  let txn = event.target.transaction;

  is(db.objectStoreNames.length, 0, "Correct objectStoreNames list");

  let objectStore = db.createObjectStore(storeName, { keyPath: "foo" });
  is(db.objectStoreNames.length, 1, "Correct objectStoreNames list");
  is(db.objectStoreNames.item(0), objectStore.name, "Correct name");

  let index1 = objectStore.createIndex(indexName1, "bar");
  is(objectStore.index(indexName1).name, index1.name, "Correct index name");
  is(index1.name, indexName1, "Correct index name");
  let index2 = objectStore.createIndex(indexName2, "baz");
  is(objectStore.index(indexName2).name, index2.name, "Correct index name");
  is(index2.name, indexName2, "Correct index name");

  txn.oncomplete = continueToNextStepSync;
  yield undefined;
  request.onsuccess = continueToNextStep;
  yield undefined;
  db.close();

  info("Verify IDB Errors in version 2.");
  request = indexedDB.open(name, 2);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  event = yield undefined;

  db = event.target.result;
  txn = event.target.transaction;

  objectStore = txn.objectStore(storeName);
  index1 = objectStore.index(indexName1);
  index2 = objectStore.index(indexName2);
  is(index1.name, indexName1, "Correct index name");
  is(index2.name, indexName2, "Correct index name");

  // Rename with the name already adopted by the other index.
  try {
    index1.name = indexName2;
    ok(false, "ConstraintError shall be thrown if the index name already exists.");
  } catch (e) {
    ok(e instanceof DOMException, "got a database exception");
    is(e.name, "ConstraintError", "correct error");
  }

  // Rename with identical name.
  try {
    index1.name = indexName1;
    ok(true, "It shall be fine to set the same name.");
  } catch (e) {
    ok(false, "Got a database exception: " + e.name);
  }

  objectStore.deleteIndex(indexName2);

  // Rename after deleted.
  try {
    index2.name = indexName2;
    ok(false, "InvalidStateError shall be thrown if deleted.");
  } catch (e) {
    ok(e instanceof DOMException, "got a database exception");
    is(e.name, "InvalidStateError", "correct error");
  }

  txn.oncomplete = continueToNextStepSync;
  yield undefined;
  request.onsuccess = continueToNextStep;
  yield undefined;
  db.close();

  // Rename when the transaction is inactive.
  try {
    index1.name = indexName1;
    ok(false, "TransactionInactiveError shall be thrown if the transaction is inactive.");
  } catch (e) {
    ok(e instanceof DOMException, "got a database exception");
    is(e.name, "TransactionInactiveError", "correct error");
  }

  info("Rename when the transaction is not an upgrade one.");
  request = indexedDB.open(name, 2);
  request.onerror = errorHandler;
  request.onupgradeneeded = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  db = event.target.result;
  txn = db.transaction(storeName);
  objectStore = txn.objectStore(storeName);
  index1 = objectStore.index(indexName1);

  try {
    index1.name = indexName1;
    ok(false, "InvalidStateError shall be thrown if it's not an upgrade transaction.");
  } catch (e) {
    ok(e instanceof DOMException, "got a database exception");
    is(e.name, "InvalidStateError", "correct error");
  }

  txn.oncomplete = continueToNextStepSync;
  yield undefined;
  db.close();

  finishTest();
  yield undefined;
}
