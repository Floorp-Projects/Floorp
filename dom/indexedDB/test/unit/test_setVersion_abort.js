/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const description = "My Test Database";

  let request = mozIndexedDB.open(name, 1, description);
  request.onerror = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield;

  let db = event.target.result;

  let objectStore = db.createObjectStore("foo");
  let index = objectStore.createIndex("bar", "baz");

  is(db.version, 1, "Correct version");
  is(db.objectStoreNames.length, 1, "Correct objectStoreNames length");
  is(objectStore.indexNames.length, 1, "Correct indexNames length");

  let transaction = event.target.transaction;
  transaction.oncomplete = unexpectedSuccessHandler;
  transaction.onabort = grabEventAndContinueHandler;
  transaction.abort();

  event = yield;
  is(event.type, "abort", "Got transaction abort event");
  is(event.target, transaction, "Right target");

  is(db.version, 1, "Correct version");
  is(db.objectStoreNames.length, 1, "Correct objectStoreNames length");

  event = yield;
  is(event.type, "error", "Got request error event");
  is(event.target, request, "Right target");

  request = mozIndexedDB.open(name, 1, description);
  request.onerror = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield;

  let db2 = event.target.result;
  
  isnot(db, db2, "Should give a different db instance");
  is(db2.version, 1, "Correct version");
  is(db2.objectStoreNames.length, 0, "Correct objectStoreNames length");

  request.onsuccess = grabEventAndContinueHandler;
  yield;

  finishTest();
  yield;
}
