/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  let request = indexedDB.open(this.window ? window.location.pathname : "Splendid Test", 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  db.onerror = errorHandler;

  request.onsuccess = continueToNextStep;

  db.createObjectStore("foo");
  yield undefined;

  let trans1 = db.transaction("foo", "readwrite");
  let trans2 = db.transaction("foo", "readwrite");

  let request1 = trans2.objectStore("foo").put("2", 42);
  let request2 = trans1.objectStore("foo").put("1", 42);

  request1.onerror = errorHandler;
  request2.onerror = errorHandler;

  trans1.oncomplete = grabEventAndContinueHandler;
  trans2.oncomplete = grabEventAndContinueHandler;

  yield undefined;
  yield undefined;

  let trans3 = db.transaction("foo", "readonly");
  request = trans3.objectStore("foo").get(42);
  request.onsuccess = grabEventAndContinueHandler;
  request.onerror = errorHandler;

  event = yield undefined;
  is(event.target.result, "2", "Transactions were ordered properly.");

  finishTest();
  yield undefined;
}

