/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testGenerator */
var testGenerator = testSteps();

function* testSteps() {
  const dbNames = ["No files", "Blobs and mutable files"];
  const version = 1;
  const objectStoreName = "test";

  clearAllDatabases(continueToNextStepSync);
  yield undefined;

  installPackagedProfile("mutableFileUpgrade_profile");

  let request = indexedDB.open(dbNames[0], version);
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  is(event.type, "success", "Correct event type");

  let db = event.target.result;
  db.onerror = errorHandler;

  request = db
    .transaction([objectStoreName])
    .objectStore(objectStoreName)
    .get(1);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, "text", "Correct result");

  request = indexedDB.open(dbNames[1], version);
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.type, "success", "Correct event type");

  db = event.target.result;
  db.onerror = errorHandler;

  request = db
    .transaction([objectStoreName])
    .objectStore(objectStoreName)
    .get(1);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, "text", "Correct result");

  request = db
    .transaction([objectStoreName])
    .objectStore(objectStoreName)
    .get(2);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  verifyBlob(event.target.result, getBlob("blob0"));
  yield undefined;

  request = db
    .transaction([objectStoreName])
    .objectStore(objectStoreName)
    .get(3);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  let result = event.target.result;

  verifyBlob(result[0], getBlob("blob1"));
  yield undefined;

  verifyBlob(result[1], getBlob("blob2"));
  yield undefined;

  request = db
    .transaction([objectStoreName])
    .objectStore(objectStoreName)
    .get(4);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  try {
    event.target.result;
    ok(false, "IDBMutableFile must not be read");
  } catch (err) {
    is(err.name, "InvalidStateError", "Wrong error type");
  }

  request = db
    .transaction([objectStoreName])
    .objectStore(objectStoreName)
    .get(5);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  try {
    event.target.result;
    ok(false, "IDBMutableFile must not be read");
  } catch (err) {
    is(err.name, "InvalidStateError", "Wrong error type");
  }

  request = db
    .transaction([objectStoreName])
    .objectStore(objectStoreName)
    .get(6);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  try {
    event.target.result;
    ok(false, "IDBMutableFile must not be read");
  } catch (err) {
    is(err.name, "InvalidStateError", "Wrong error type");
  }

  finishTest();
  yield undefined;
}
