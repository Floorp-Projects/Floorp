/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onsuccess = unexpectedSuccessHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;

  let objectStore = db.createObjectStore("foo");
  objectStore.createIndex("bar", "baz");

  is(db.version, 1, "Correct version");
  is(db.objectStoreNames.length, 1, "Correct objectStoreNames length");
  is(objectStore.indexNames.length, 1, "Correct indexNames length");

  let transaction = event.target.transaction;
  is(transaction.mode, "versionchange", "Correct transaction mode");
  transaction.oncomplete = unexpectedSuccessHandler;
  transaction.onabort = grabEventAndContinueHandler;
  transaction.abort();

  is(db.version, 0, "Correct version");
  is(db.objectStoreNames.length, 0, "Correct objectStoreNames length");
  is(objectStore.indexNames.length, 0, "Correct indexNames length");

  // Test that the db is actually closed.
  try {
    db.transaction("");
    ok(false, "Expect an exception");
  } catch (e) {
    ok(true, "Expect an exception");
    is(e.name, "InvalidStateError", "Expect an InvalidStateError");
  }

  event = yield undefined;
  is(event.type, "abort", "Got transaction abort event");
  is(event.target, transaction, "Right target");

  is(db.version, 0, "Correct version");
  is(db.objectStoreNames.length, 0, "Correct objectStoreNames length");
  is(objectStore.indexNames.length, 0, "Correct indexNames length");

  request.onerror = grabEventAndContinueHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;

  event = yield undefined;

  is(event.type, "error", "Got request error event");
  is(event.target, request, "Right target");
  is(event.target.transaction, null, "No transaction");

  event.preventDefault();

  request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onsuccess = unexpectedSuccessHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.type, "upgradeneeded", "Got upgradeneeded event");

  let db2 = event.target.result;

  isnot(db, db2, "Should give a different db instance");
  is(db2.version, 1, "Correct version");
  is(db2.objectStoreNames.length, 0, "Correct objectStoreNames length");

  let objectStore2 = db2.createObjectStore("foo");
  objectStore2.createIndex("bar", "baz");

  request.onsuccess = grabEventAndContinueHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  event = yield undefined;

  is(event.target.result, db2, "Correct target");
  is(event.type, "success", "Got success event");
  is(db2.version, 1, "Correct version");
  is(db2.objectStoreNames.length, 1, "Correct objectStoreNames length");
  is(objectStore2.indexNames.length, 1, "Correct indexNames length");
  is(db.version, 0, "Correct version still");
  is(db.objectStoreNames.length, 0, "Correct objectStoreNames length still");
  is(objectStore.indexNames.length, 0, "Correct indexNames length still");

  finishTest();
}
