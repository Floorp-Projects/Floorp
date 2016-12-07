/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const storeName_ToBeDeleted = "test store to be deleted";
  const storeName_v0 = "test store v0";
  const storeName_v1 = "test store v1";
  const storeName_v2 = "test store v2";
  const storeName_v3 = storeName_ToBeDeleted;
  const storeName_v4 = "test store v4";

  info("Rename in v1.");
  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;

  let db = event.target.result;
  let txn = event.target.transaction;

  is(db.objectStoreNames.length, 0, "Correct objectStoreNames list");

  // create objectstore to be deleted later in v3.
  db.createObjectStore(storeName_ToBeDeleted, { keyPath: "foo" });
  is(db.objectStoreNames.length, 1, "Correct objectStoreNames list");
  ok(db.objectStoreNames.contains(storeName_ToBeDeleted), "Correct name");

  // create target objectstore to be renamed.
  let objectStore = db.createObjectStore(storeName_v0, { keyPath: "bar" });
  is(db.objectStoreNames.length, 2, "Correct objectStoreNames list");
  ok(db.objectStoreNames.contains(objectStore.name), "Correct name");

  objectStore.name = storeName_v1;
  is(objectStore.name, storeName_v1, "Renamed successfully");

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

  is(db.objectStoreNames.length, 2, "Correct objectStoreNames list");
  ok(db.objectStoreNames.contains(storeName_v1), "Correct name");
  ok(db.objectStoreNames.contains(storeName_ToBeDeleted), "Correct name");

  objectStore = txn.objectStore(storeName_v1);
  objectStore.name = storeName_v2;
  is(objectStore.name, storeName_v2, "Renamed successfully");

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

  is(db.objectStoreNames.length, 2, "Correct objectStoreNames list");
  ok(db.objectStoreNames.contains(storeName_v2), "Correct name");
  ok(db.objectStoreNames.contains(storeName_ToBeDeleted), "Correct name");

  db.close();

  info("Rename in v3.");
  request = indexedDB.open(name, 3);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  event = yield undefined;

  db = event.target.result;
  txn = event.target.transaction;

  is(db.objectStoreNames.length, 2, "Correct objectStoreNames list");
  ok(db.objectStoreNames.contains(storeName_v2), "Correct name");
  ok(db.objectStoreNames.contains(storeName_ToBeDeleted), "Correct name");
  db.deleteObjectStore(storeName_ToBeDeleted);
  is(db.objectStoreNames.length, 1, "Correct objectStoreNames list");
  ok(db.objectStoreNames.contains(storeName_v2) &&
     !db.objectStoreNames.contains(storeName_ToBeDeleted), "Deleted correctly");

  objectStore = txn.objectStore(storeName_v2);
  objectStore.name = storeName_v3;
  is(objectStore.name, storeName_v3, "Renamed successfully");

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

  is(db.objectStoreNames.length, 1, "Correct objectStoreNames list");
  ok(db.objectStoreNames.contains(storeName_v3), "Correct name");

  db.close();

  info("Abort the version change transaction while renaming objectstore.");
  request = indexedDB.open(name, 4);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  event = yield undefined;

  db = event.target.result;
  txn = event.target.transaction;

  objectStore = txn.objectStore(storeName_v3);
  objectStore.name = storeName_v4;
  is(objectStore.name, storeName_v4, "Renamed successfully");
  let putRequest = objectStore.put({ bar: "barValue" });
  putRequest.onsuccess = continueToNextStepSync;
  yield undefined;

  // Aborting the transaction.
  request.onerror = expectedErrorHandler("AbortError");
  txn.abort();
  yield undefined;

  // Verify if the name of the objectStore handle is reverted.
  is(objectStore.name, storeName_v3, "The name is reverted after aborted.");

  info("Verify if the objectstore name is unchanged.");
  request = indexedDB.open(name, 3);
  request.onerror = errorHandler;
  request.onupgradeneeded = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  db = event.target.result;

  is(db.objectStoreNames.length, 1, "Correct objectStoreNames list");
  ok(db.objectStoreNames.contains(storeName_v3), "Correct name");

  db.close();

  finishTest();
}
