/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const storeName_ToBeDeleted = "test store to be deleted";

  info("Create objectStore in v1.");
  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;

  let db = event.target.result;
  let txn = event.target.transaction;

  is(db.objectStoreNames.length, 0, "Correct objectStoreNames list");

  // create objectstore to be deleted later in v2.
  db.createObjectStore(storeName_ToBeDeleted, { keyPath: "foo" });
  is(db.objectStoreNames.length, 1, "Correct objectStoreNames list");
  ok(db.objectStoreNames.contains(storeName_ToBeDeleted), "Correct name");

  txn.oncomplete = continueToNextStepSync;
  yield undefined;
  request.onsuccess = continueToNextStep;
  yield undefined;
  db.close();

  info("Delete objectStore in v2.");
  request = indexedDB.open(name, 2);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  event = yield undefined;

  db = event.target.result;
  txn = event.target.transaction;

  let objectStore = txn.objectStore(storeName_ToBeDeleted);
  ok(objectStore, "objectStore is available");

  db.deleteObjectStore(storeName_ToBeDeleted);

  // Aborting the transaction.
  request.onerror = expectedErrorHandler("AbortError");
  txn.abort();
  try {
    objectStore.get("foo");
    ok(false, "TransactionInactiveError shall be thrown if the transaction is inactive.");
  } catch (e) {
    ok(e instanceof DOMException, "got a database exception");
    is(e.name, "TransactionInactiveError", "correct error");
  }

  yield undefined;

  try {
    objectStore.get("foo");
    ok(false, "TransactionInactiveError shall be thrown if the transaction is inactive.");
  } catch (e) {
    ok(e instanceof DOMException, "got a database exception");
    is(e.name, "TransactionInactiveError", "correct error");
  }

  finishTest();
}
