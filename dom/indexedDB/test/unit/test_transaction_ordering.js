/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  let request = mozIndexedDB.open(this.window ? window.location.pathname : "Splendid Test", 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield;

  let db = event.target.result;
  db.onerror = errorHandler;

  request.onsuccess = continueToNextStep;

  db.createObjectStore("foo");
  yield;

  let trans1 = db.transaction("foo", IDBTransaction.READ_WRITE);
  let trans2 = db.transaction("foo", IDBTransaction.READ_WRITE);

  let request1 = trans2.objectStore("foo").put("2", 42);
  let request2 = trans1.objectStore("foo").put("1", 42);

  request1.onerror = errorHandler;
  request2.onerror = errorHandler;

  trans1.oncomplete = grabEventAndContinueHandler;
  trans2.oncomplete = grabEventAndContinueHandler;

  yield;
  yield;

  let trans3 = db.transaction("foo", IDBTransaction.READ);
  let request = trans3.objectStore("foo").get(42);
  request.onsuccess = grabEventAndContinueHandler;
  request.onerror = errorHandler;

  let event = yield;
  is(event.target.result, "2", "Transactions were ordered properly.");

  finishTest();
  yield;
}

