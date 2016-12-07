/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps() {
  const dbName = this.window ?
                 window.location.pathname :
                 "test_transaction_error";
  const dbVersion = 1;
  const objectStoreName = "foo";
  const data = { };
  const dataKey = 1;
  const expectedError = "ConstraintError";

  let request = indexedDB.open(dbName, dbVersion);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;

  let event = yield undefined;

  info("Creating database");

  let db = event.target.result;
  let objectStore = db.createObjectStore(objectStoreName);
  objectStore.add(data, dataKey);

  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  db = event.target.result;

  try {
    db.transaction(objectStoreName, "versionchange");
    ok(false, "TypeError shall be thrown if transaction mode is wrong.");
  } catch (e) {
    ok(e instanceof DOMException, "got a database exception");
    is(e.name, "TypeError", "correct error");
  }

  let transaction = db.transaction(objectStoreName, "readwrite");
  transaction.onerror = grabEventAndContinueHandler;
  transaction.oncomplete = grabEventAndContinueHandler;

  objectStore = transaction.objectStore(objectStoreName);

  info("Adding duplicate entry with preventDefault()");

  request = objectStore.add(data, dataKey);
  request.onsuccess = unexpectedSuccessHandler;
  request.onerror = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.type, "error", "Got an error event");
  is(event.target, request, "Error event targeted request");
  is(event.currentTarget, request, "Got request error first");
  is(event.currentTarget.error.name, expectedError,
     "Request has correct error");
  event.preventDefault();

  event = yield undefined;

  is(event.type, "error", "Got an error event");
  is(event.target, request, "Error event targeted request");
  is(event.currentTarget, transaction, "Got transaction error second");
  is(event.currentTarget.error, null, "Transaction has null error");

  event = yield undefined;

  is(event.type, "complete", "Got a complete event");
  is(event.target, transaction, "Complete event targeted transaction");
  is(event.currentTarget, transaction,
     "Complete event only targeted transaction");
  is(event.currentTarget.error, null, "Transaction has null error");

  // Try again without preventDefault().

  transaction = db.transaction(objectStoreName, "readwrite");
  transaction.onerror = grabEventAndContinueHandler;
  transaction.onabort = grabEventAndContinueHandler;

  objectStore = transaction.objectStore(objectStoreName);

  info("Adding duplicate entry without preventDefault()");

  if ("SimpleTest" in this) {
    SimpleTest.expectUncaughtException();
  } else if ("DedicatedWorkerGlobalScope" in self &&
             self instanceof DedicatedWorkerGlobalScope) {
    let oldErrorFunction = self.onerror;
    self.onerror = function(message, file, line) {
      self.onerror = oldErrorFunction;
      oldErrorFunction = null;

      is(message,
        "ConstraintError",
        "Got expected ConstraintError on DedicatedWorkerGlobalScope");
      return true;
    };
  }

  request = objectStore.add(data, dataKey);
  request.onsuccess = unexpectedSuccessHandler;
  request.onerror = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.type, "error", "Got an error event");
  is(event.target, request, "Error event targeted request");
  is(event.currentTarget, request, "Got request error first");
  is(event.currentTarget.error.name, expectedError,
     "Request has correct error");

  event = yield undefined;

  is(event.type, "error", "Got an error event");
  is(event.target, request, "Error event targeted request");
  is(event.currentTarget, transaction, "Got transaction error second");
  is(event.currentTarget.error, null, "Transaction has null error");

  event = yield undefined;

  is(event.type, "abort", "Got an abort event");
  is(event.target, transaction, "Abort event targeted transaction");
  is(event.currentTarget, transaction,
     "Abort event only targeted transaction");
  is(event.currentTarget.error.name, expectedError,
     "Transaction has correct error");

  finishTest();
}
