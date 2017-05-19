/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

var abortFired = false;

function abortListener(evt)
{
  abortFired = true;
  is(evt.target.error, null, "Expect a null error for an aborted transaction");
}

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  db.onabort = abortListener;

  let transaction;
  let objectStore;
  let index;

  transaction = event.target.transaction;

  is(transaction.error, null, "Expect a null error");

  objectStore = db.createObjectStore("foo", { autoIncrement: true });
  index = objectStore.createIndex("fooindex", "indexKey", { unique: true });

  is(transaction.db, db, "Correct database");
  is(transaction.mode, "versionchange", "Correct mode");
  is(transaction.objectStoreNames.length, 1, "Correct names length");
  is(transaction.objectStoreNames.item(0), "foo", "Correct name");
  is(transaction.objectStore("foo"), objectStore, "Can get stores");
  is(transaction.oncomplete, null, "No complete listener");
  is(transaction.onabort, null, "No abort listener");

  is(objectStore.name, "foo", "Correct name");
  is(objectStore.keyPath, null, "Correct keyPath");

  is(objectStore.indexNames.length, 1, "Correct indexNames length");
  is(objectStore.indexNames[0], "fooindex", "Correct indexNames name");
  is(objectStore.index("fooindex"), index, "Can get index");

  // Wait until it's complete!
  transaction.oncomplete = grabEventAndContinueHandler;
  event = yield undefined;

  is(transaction.db, db, "Correct database");
  is(transaction.mode, "versionchange", "Correct mode");
  is(transaction.objectStoreNames.length, 1, "Correct names length");
  is(transaction.objectStoreNames.item(0), "foo", "Correct name");
  is(transaction.onabort, null, "No abort listener");

  try {
    is(transaction.objectStore("foo").name, "foo", "Can't get stores");
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Out of scope transaction can't make stores");
  }

  is(objectStore.name, "foo", "Correct name");
  is(objectStore.keyPath, null, "Correct keyPath");

  is(objectStore.indexNames.length, 1, "Correct indexNames length");
  is(objectStore.indexNames[0], "fooindex", "Correct indexNames name");

  try {
    objectStore.add({});
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Add threw");
  }

  try {
    objectStore.put({}, 1);
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Put threw");
  }

  try {
    objectStore.put({}, 1);
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Put threw");
  }

  try {
    objectStore.delete(1);
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Remove threw");
  }

  try {
    objectStore.get(1);
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Get threw");
  }

  try {
    objectStore.getAll(null);
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "GetAll threw");
  }

  try {
    objectStore.openCursor();
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "OpenCursor threw");
  }

  try {
    objectStore.createIndex("bar", "id");
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "CreateIndex threw");
  }

  try {
    objectStore.index("bar");
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Index threw");
  }

  try {
    objectStore.deleteIndex("bar");
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "RemoveIndex threw");
  }

  yield undefined;

  request = db.transaction("foo", "readwrite").objectStore("foo").add({});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  event.target.transaction.onabort = function(event) {
    ok(false, "Shouldn't see an abort event!");
  };
  event.target.transaction.oncomplete = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.type, "complete", "Right kind of event");

  let key;

  request = db.transaction("foo", "readwrite").objectStore("foo").add({});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  key = event.target.result;

  event.target.transaction.onabort = grabEventAndContinueHandler;
  event.target.transaction.oncomplete = function(event) {
    ok(false, "Shouldn't see a complete event here!");
  };

  event.target.transaction.abort();

  event = yield undefined;

  is(event.type, "abort", "Right kind of event");

  request = db.transaction("foo").objectStore("foo").get(key);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, undefined, "Object was removed");

  executeSoon(function() { testGenerator.next(); });
  yield undefined;

  let keys = [];
  let abortEventCount = 0;
  function abortErrorHandler(event) {
      is(event.target.error.name, "AbortError",
         "Good error");
      abortEventCount++;
      event.preventDefault();
  }
  objectStore = db.transaction("foo", "readwrite").objectStore("foo");

  for (let i = 0; i < 10; i++) {
    request = objectStore.add({});
    request.onerror = abortErrorHandler;
    request.onsuccess = function(event) {
      keys.push(event.target.result);
      if (keys.length == 5) {
        event.target.transaction.onabort = grabEventAndContinueHandler;
        event.target.transaction.abort();
      }
    };
  }
  event = yield undefined;

  is(event.type, "abort", "Got abort event");
  is(keys.length, 5, "Added 5 items in this transaction");
  is(abortEventCount, 5, "Got 5 abort error events");

  for (let i in keys) {
    request = db.transaction("foo").objectStore("foo").get(keys[i]);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.target.result, undefined, "Object was removed by abort");
  }

  // Set up some predictible data
  transaction = db.transaction("foo", "readwrite");
  objectStore = transaction.objectStore("foo");
  objectStore.clear();
  objectStore.add({}, 1);
  objectStore.add({}, 2);
  request = objectStore.add({}, 1);
  request.onsuccess = function() {
    ok(false, "inserting duplicate key should fail");
  }
  request.onerror = function(event) {
    ok(true, "inserting duplicate key should fail");
    event.preventDefault();
  }
  transaction.oncomplete = grabEventAndContinueHandler;
  yield undefined;

  // Check when aborting is allowed
  abortEventCount = 0;
  let expectedAbortEventCount = 0;

  // During INITIAL
  transaction = db.transaction("foo");
  transaction.abort();
  try {
    transaction.abort();
    ok(false, "second abort should throw an error");
  }
  catch (ex) {
    ok(true, "second abort should throw an error");
  }

  // During LOADING
  transaction = db.transaction("foo");
  transaction.objectStore("foo").get(1).onerror = abortErrorHandler;
  expectedAbortEventCount++;
  transaction.abort();
  try {
    transaction.abort();
    ok(false, "second abort should throw an error");
  }
  catch (ex) {
    ok(true, "second abort should throw an error");
  }

  // During LOADING from callback
  transaction = db.transaction("foo");
  transaction.objectStore("foo").get(1).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;
  transaction.objectStore("foo").get(1).onerror = abortErrorHandler;
  expectedAbortEventCount++
  transaction.abort();
  try {
    transaction.abort();
    ok(false, "second abort should throw an error");
  }
  catch (ex) {
    ok(true, "second abort should throw an error");
  }

  // During LOADING from error callback
  transaction = db.transaction("foo", "readwrite");
  transaction.objectStore("foo").add({}, 1).onerror = function(event) {
    event.preventDefault();

    transaction.objectStore("foo").get(1).onerror = abortErrorHandler;
    expectedAbortEventCount++

    transaction.abort();
    continueToNextStep();
  }
  yield undefined;

  // In between callbacks
  transaction = db.transaction("foo");
  function makeNewRequest() {
    let r = transaction.objectStore("foo").get(1);
    r.onsuccess = makeNewRequest;
    r.onerror = abortErrorHandler;
  }
  makeNewRequest();
  transaction.objectStore("foo").get(1).onsuccess = function(event) {
    executeSoon(function() {
      transaction.abort();
      expectedAbortEventCount++;
      continueToNextStep();
    });
  };
  yield undefined;

  // During COMMITTING
  transaction = db.transaction("foo", "readwrite");
  transaction.objectStore("foo").put({hello: "world"}, 1).onsuccess = function(event) {
    continueToNextStep();
  };
  yield undefined;
  try {
    transaction.abort();
    ok(false, "second abort should throw an error");
  }
  catch (ex) {
    ok(true, "second abort should throw an error");
  }
  transaction.oncomplete = grabEventAndContinueHandler;
  event = yield undefined;

  // Since the previous transaction shouldn't have caused any error events,
  // we know that all events should have fired by now.
  is(abortEventCount, expectedAbortEventCount,
     "All abort errors fired");

  // Abort both failing and succeeding requests
  transaction = db.transaction("foo", "readwrite");
  transaction.onabort = transaction.oncomplete = grabEventAndContinueHandler;
  transaction.objectStore("foo").add({indexKey: "key"}).onsuccess = function(event) {
    transaction.abort();
  };
  let request1 = transaction.objectStore("foo").add({indexKey: "key"});
  request1.onsuccess = grabEventAndContinueHandler;
  request1.onerror = grabEventAndContinueHandler;
  let request2 = transaction.objectStore("foo").get(1);
  request2.onsuccess = grabEventAndContinueHandler;
  request2.onerror = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.type, "error", "abort() should make all requests fail");
  is(event.target, request1, "abort() should make all requests fail");
  is(event.target.error.name, "AbortError", "abort() should make all requests fail");
  event.preventDefault();

  event = yield undefined;
  is(event.type, "error", "abort() should make all requests fail");
  is(event.target, request2, "abort() should make all requests fail");
  is(event.target.error.name, "AbortError", "abort() should make all requests fail");
  event.preventDefault();

  event = yield undefined;
  is(event.type, "abort", "transaction should fail");
  is(event.target, transaction, "transaction should fail");

  ok(abortFired, "Abort should have fired!");

  finishTest();
}
