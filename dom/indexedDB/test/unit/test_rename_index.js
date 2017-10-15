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
  const indexName_v0 = "test index v0";
  const indexName_v1 = "test index v1";
  const indexName_v2 = "test index v2";
  const indexName_v3 = indexName_ToBeDeleted;
  const indexName_v4 = "test index v4";

  info("Rename in v1.");
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

  // create index to be deleted later in v3.
  objectStore.createIndex(indexName_ToBeDeleted, "foo");
  ok(objectStore.index(indexName_ToBeDeleted), "Index created.");

  // create target index to be renamed.
  let index = objectStore.createIndex(indexName_v0, "bar");
  ok(objectStore.index(indexName_v0), "Index created.");
  is(index.name, indexName_v0, "Correct index name");
  index.name = indexName_v1;
  is(index.name, indexName_v1, "Renamed index successfully");

  txn.oncomplete = continueToNextStepSync;
  yield undefined;
  request.onsuccess = continueToNextStep;
  yield undefined;
  db.close();

  info("Verify renaming done in v1 and run renaming in v2.");
  request = indexedDB.open(name, 2);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  event = yield undefined;

  db = event.target.result;
  txn = event.target.transaction;

  objectStore = txn.objectStore(storeName);

  // indexName_v0 created in v1 shall not be available.
  try {
    index = objectStore.index(indexName_v0);
    ok(false, "NotFoundError shall be thrown.");
  } catch (e) {
    ok(e instanceof DOMException, "got a database exception");
    is(e.name, "NotFoundError", "correct error");
  }

  // rename to "v2".
  index = objectStore.index(indexName_v1);
  is(index.name, indexName_v1, "Correct index name");
  index.name = indexName_v2;
  is(index.name, indexName_v2, "Renamed index successfully");

  txn.oncomplete = continueToNextStepSync;
  yield undefined;
  request.onsuccess = continueToNextStep;
  yield undefined;
  db.close();

  info("Verify renaming done in v2.");
  request = indexedDB.open(name, 2);
  request.onerror = errorHandler;
  request.onupgradeneeded = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  db = event.target.result;
  txn = db.transaction(storeName);

  objectStore = txn.objectStore(storeName);
  index = objectStore.index(indexName_v2);
  is(index.name, indexName_v2, "Correct index name");

  db.close();

  info("Rename in v3.");
  request = indexedDB.open(name, 3);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  event = yield undefined;

  db = event.target.result;
  txn = event.target.transaction;

  objectStore = txn.objectStore(storeName);
  ok(objectStore.index(indexName_ToBeDeleted), "index is valid.");
  objectStore.deleteIndex(indexName_ToBeDeleted);
  try {
    objectStore.index(indexName_ToBeDeleted);
    ok(false, "NotFoundError shall be thrown if the index name is deleted.");
  } catch (e) {
    ok(e instanceof DOMException, "got a database exception");
    is(e.name, "NotFoundError", "correct error");
  }

  info("Rename with the name of the deleted index.");
  index = objectStore.index(indexName_v2);
  index.name = indexName_v3;
  is(index.name, indexName_v3, "Renamed index successfully");

  txn.oncomplete = continueToNextStepSync;
  yield undefined;
  request.onsuccess = continueToNextStep;
  yield undefined;
  db.close();

  info("Verify renaming done in v3.");
  request = indexedDB.open(name, 3);
  request.onerror = errorHandler;
  request.onupgradeneeded = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  db = event.target.result;
  txn = db.transaction(storeName);

  objectStore = txn.objectStore(storeName);
  index = objectStore.index(indexName_v3);
  is(index.name, indexName_v3, "Correct index name");

  db.close();

  info("Abort the version change transaction while renaming index.");
  request = indexedDB.open(name, 4);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  event = yield undefined;

  db = event.target.result;
  txn = event.target.transaction;

  objectStore = txn.objectStore(storeName);
  index = objectStore.index(indexName_v3);
  index.name = indexName_v4;
  is(index.name, indexName_v4, "Renamed successfully");
  let putRequest = objectStore.put({ foo: "fooValue", bar: "barValue" });
  putRequest.onsuccess = continueToNextStepSync;
  yield undefined;

  // Aborting the transaction.
  request.onerror = expectedErrorHandler("AbortError");
  txn.abort();
  yield undefined;

  // Verify if the name of the index handle is reverted.
  is(index.name, indexName_v3, "The name is reverted after aborted.");

  info("Verify if the objectstore name is unchanged.");
  request = indexedDB.open(name, 3);
  request.onerror = errorHandler;
  request.onupgradeneeded = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  db = event.target.result;
  txn = db.transaction(storeName);

  objectStore = txn.objectStore(storeName);
  index = objectStore.index(indexName_v3);
  is(index.name, indexName_v3, "Correct index name");

  db.close();

  finishTest();
}
