/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const storeName = "test store";
  const indexName_ToBeDeleted = "test index to be deleted";

  info("Create index in v1.");
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
  is(db.objectStoreNames.item(0), objectStore.name, "Correct object store name");

  // create index to be deleted later in v2.
  objectStore.createIndex(indexName_ToBeDeleted, "foo");
  ok(objectStore.index(indexName_ToBeDeleted), "Index created.");

  txn.oncomplete = continueToNextStepSync;
  yield undefined;
  request.onsuccess = continueToNextStep;
  yield undefined;
  db.close();

  info("Delete index in v2.");
  request = indexedDB.open(name, 2);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  event = yield undefined;

  db = event.target.result;
  txn = event.target.transaction;

  objectStore = txn.objectStore(storeName);
  let index = objectStore.index(indexName_ToBeDeleted);
  ok(index, "index is valid.");
  objectStore.deleteIndex(indexName_ToBeDeleted);

  // Aborting the transaction.
  request.onerror = expectedErrorHandler("AbortError");
  txn.abort();
  try {
    index.get("foo");
    ok(false, "TransactionInactiveError shall be thrown right after a deletion of an index is aborted.");
  } catch (e) {
    ok(e instanceof DOMException, "got a database exception");
    is(e.name, "TransactionInactiveError", "TransactionInactiveError shall be thrown right after a deletion of an index is aborted.");
  }

  yield undefined;

  try {
    index.get("foo");
    ok(false, "TransactionInactiveError shall be thrown after the transaction is inactive.");
  } catch (e) {
    ok(e instanceof DOMException, "got a database exception");
    is(e.name, "TransactionInactiveError", "TransactionInactiveError shall be thrown after the transaction is inactive.");
  }

  finishTest();
}
