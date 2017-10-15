/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  function testInvalidStateError(aDb, aTxn) {
    try {
      info("The db shall become invalid after closed.");
      aDb.transaction("store");
      ok(false, "InvalidStateError shall be thrown.");
    } catch (e) {
      ok(e instanceof DOMException, "got a database exception");
      is(e.name, "InvalidStateError", "correct error");
    }

    try {
      info("The txn shall become invalid after closed.");
      aTxn.objectStore("store");
      ok(false, "InvalidStateError shall be thrown.");
    } catch (e) {
      ok(e instanceof DOMException, "got a database exception");
      is(e.name, "InvalidStateError", "correct error");
    }
  }

  const name = this.window ? window.location.pathname :
                             "test_database_onclose.js";

  info("#1: Verifying IDBDatabase.onclose after cleared by the agent.");
  let openRequest = indexedDB.open(name, 1);
  openRequest.onerror = errorHandler;
  openRequest.onsuccess = unexpectedSuccessHandler;
  openRequest.onupgradeneeded = grabEventAndContinueHandler;

  ok(openRequest instanceof IDBOpenDBRequest, "Expect an IDBOpenDBRequest");

  let event = yield undefined;

  is(event.type, "upgradeneeded", "Expect an upgradeneeded event");
  ok(event instanceof IDBVersionChangeEvent, "Expect a versionchange event");

  let db = event.target.result;
  db.createObjectStore("store");

  openRequest.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  is(event.type, "success", "Expect a success event");
  is(event.target, openRequest, "Event has right target");
  ok(event.target.result instanceof IDBDatabase, "Result should be a database");
  is(db.objectStoreNames.length, 1, "Expect an objectStore here");

  let txn = db.transaction("store", "readwrite");
  let objectStore = txn.objectStore("store");

  clearAllDatabases(continueToNextStep);

  db.onclose = grabEventAndContinueHandler;
  event = yield undefined;
  is(event.type, "close", "Expect a close event");
  is(event.target, db, "Correct target");

  info("Wait for callback of clearAllDatabases().");
  yield undefined;

  testInvalidStateError(db, txn);

  info("#2: Verifying IDBDatabase.onclose && IDBTransaction.onerror " +
       "in *write* operation after cleared by the agent.");
  openRequest = indexedDB.open(name, 1);
  openRequest.onerror = errorHandler;
  openRequest.onsuccess = unexpectedSuccessHandler;
  openRequest.onupgradeneeded = grabEventAndContinueHandler;

  ok(openRequest instanceof IDBOpenDBRequest, "Expect an IDBOpenDBRequest");

  event = yield undefined;

  is(event.type, "upgradeneeded", "Expect an upgradeneeded event");
  ok(event instanceof IDBVersionChangeEvent, "Expect a versionchange event");

  db = event.target.result;
  db.createObjectStore("store");

  openRequest.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  is(event.type, "success", "Expect a success event");
  is(event.target, openRequest, "Event has right target");
  ok(event.target.result instanceof IDBDatabase, "Result should be a database");
  is(db.objectStoreNames.length, 1, "Expect an objectStore here");

  txn = db.transaction("store", "readwrite");
  objectStore = txn.objectStore("store");

  let objectId = 0;
  while (true) {
    let addRequest = objectStore.add({foo: "foo"}, objectId);
    addRequest.onerror = function(event) {
      info("addRequest.onerror, objectId: " + objectId);
      txn.onerror = grabEventAndContinueHandler;
      testGenerator.next(true);
    };
    addRequest.onsuccess = function() {
      testGenerator.next(false);
    };

    if (objectId == 0) {
      clearAllDatabases(() => {
        info("clearAllDatabases is done.");
        continueToNextStep();
      });
    }

    objectId++;

    let aborted = yield undefined;
    if (aborted) {
      break;
    }
  }

  event = yield undefined;
  is(event.type, "error", "Got an error event");
  is(event.target.error.name, "AbortError", "Expected AbortError was thrown.");
  event.preventDefault();

  txn.onabort = grabEventAndContinueHandler;
  event = yield undefined;
  is(event.type, "abort", "Got an abort event");
  is(event.target.error.name, "AbortError", "Expected AbortError was thrown.");

  db.onclose = grabEventAndContinueHandler;
  event = yield undefined;
  is(event.type, "close", "Expect a close event");
  is(event.target, db, "Correct target");
  testInvalidStateError(db, txn);

  info("Wait for the callback of clearAllDatabases().");
  yield undefined;

  info("#3: Verifying IDBDatabase.onclose && IDBTransaction.onerror " +
  "in *read* operation after cleared by the agent.");
  openRequest = indexedDB.open(name, 1);
  openRequest.onerror = errorHandler;
  openRequest.onsuccess = unexpectedSuccessHandler;
  openRequest.onupgradeneeded = grabEventAndContinueHandler;

  ok(openRequest instanceof IDBOpenDBRequest, "Expect an IDBOpenDBRequest");

  event = yield undefined;

  is(event.type, "upgradeneeded", "Expect an upgradeneeded event");
  ok(event instanceof IDBVersionChangeEvent, "Expect a versionchange event");

  db = event.target.result;
  objectStore =
    db.createObjectStore("store", { keyPath: "id", autoIncrement: true });
  // The number of read records varies between 1~2000 before the db is cleared
  // during testing.
  let numberOfObjects = 3000;
  objectId = 0;
  while (true) {
    let addRequest = objectStore.add({foo: "foo"});
    addRequest.onsuccess = function() {
      objectId++;
      testGenerator.next(objectId == numberOfObjects);
    };
    addRequest.onerror = errorHandler;

    let done = yield undefined;
    if (done) {
      break;
    }
  }

  openRequest.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  is(event.type, "success", "Expect a success event");
  is(event.target, openRequest, "Event has right target");
  ok(event.target.result instanceof IDBDatabase, "Result should be a database");
  is(db.objectStoreNames.length, 1, "Expect an objectStore here");

  txn = db.transaction("store");
  objectStore = txn.objectStore("store");

  let numberOfReadObjects = 0;
  let readRequest = objectStore.openCursor();
  readRequest.onerror = function(event) {
    info("readRequest.onerror, numberOfReadObjects: " + numberOfReadObjects);
    testGenerator.next(true);
  };
  readRequest.onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      numberOfReadObjects++;
      event.target.result.continue();
    } else {
      info("Cursor is invalid, numberOfReadObjects: " + numberOfReadObjects);
      todo(false, "All records are iterated before database is cleared!");
      testGenerator.next(false);
    }
  };

  clearAllDatabases(() => {
    info("clearAllDatabases is done.");
    continueToNextStep();
  });

  let readRequestError = yield undefined;
  if (readRequestError) {
    txn.onerror = grabEventAndContinueHandler;

    event = yield undefined;
    is(event.type, "error", "Got an error event");
    is(event.target.error.name, "AbortError", "Expected AbortError was thrown.");
    event.preventDefault();

    txn.onabort = grabEventAndContinueHandler;
    event = yield undefined;
    is(event.type, "abort", "Got an abort event");
    is(event.target.error.name, "AbortError", "Expected AbortError was thrown.");

    db.onclose = grabEventAndContinueHandler;
    event = yield undefined;
    is(event.type, "close", "Expect a close event");
    is(event.target, db, "Correct target");

    testInvalidStateError(db, txn);
  }

  info("Wait for the callback of clearAllDatabases().");
  yield undefined;

  finishTest();
}
