/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const storeName1 = "test store 1";
  const storeName2 = "test store 2";

  info("Setup test object stores.");
  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;

  let db = event.target.result;
  let txn = event.target.transaction;

  is(db.objectStoreNames.length, 0, "Correct objectStoreNames list");

  let objectStore1 = db.createObjectStore(storeName1, { keyPath: "foo" });
  is(db.objectStoreNames.length, 1, "Correct objectStoreNames list");
  is(db.objectStoreNames.item(0), objectStore1.name, "Correct name");
  is(objectStore1.name, storeName1, "Correct name");

  let objectStore2 = db.createObjectStore(storeName2, { keyPath: "bar" });
  is(db.objectStoreNames.length, 2, "Correct objectStoreNames list");
  is(db.objectStoreNames.item(1), objectStore2.name, "Correct name");
  is(objectStore2.name, storeName2, "Correct name");

  txn.oncomplete = continueToNextStepSync;
  yield undefined;
  request.onsuccess = continueToNextStep;
  yield undefined;
  db.close();

  info("Verify IDB Errors in version 2.");
  request = indexedDB.open(name, 2);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = continueToNextStep;
  event = yield undefined;

  db = event.target.result;
  txn = event.target.transaction;

  is(db.objectStoreNames.length, 2, "Correct objectStoreNames list");

  objectStore1 = txn.objectStore(storeName1);
  objectStore2 = txn.objectStore(storeName2);
  is(objectStore1.name, storeName1, "Correct name");
  is(objectStore2.name, storeName2, "Correct name");

  // Rename with the name already adopted by the other object store.
  try {
    objectStore1.name = storeName2;
    ok(false, "ConstraintError shall be thrown if the store name already exists.");
  } catch (e) {
    ok(e instanceof DOMException, "got a database exception");
    is(e.name, "ConstraintError", "correct error");
  }

  // Rename with the identical name.
  try {
    objectStore1.name = storeName1;
    ok(true, "It shall be fine to set the same name.");
  } catch (e) {
    ok(false, "Got a database exception: " + e.name);
  }

  db.deleteObjectStore(storeName2);

  // Rename after deleted.
  try {
    objectStore2.name = storeName2;
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

  info("Rename when the transaction is inactive.");
  try {
    objectStore1.name = storeName1;
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
  txn = db.transaction(storeName1);
  objectStore1 = txn.objectStore(storeName1);

  try {
    objectStore1.name = storeName1;
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
