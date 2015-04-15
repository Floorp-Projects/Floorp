/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var disableWorkerTest =
  "This test requires a precise 'executeSoon()' to complete reliably. On a " +
  "worker 'executeSoon()' currently uses 'setTimeout()', and that switches " +
  "to the timer thread and back before completing. That gives the IndexedDB " +
  "transaction thread time to fully complete transactions and to place " +
  "'complete' events in the worker thread's queue before the timer event, " +
  "causing ordering problems in the spot marked 'Worker Fails Here' below.";

var testGenerator = testSteps();

function testSteps()
{
  let request = indexedDB.open(this.window ? window.location.pathname : "Splendid Test", 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;

  let db = event.target.result;
  db.onerror = errorHandler;

  event.target.transaction.onerror = errorHandler;
  event.target.transaction.oncomplete = grabEventAndContinueHandler;

  let os = db.createObjectStore("foo", { autoIncrement: true });
  let index = os.createIndex("bar", "foo.bar");
  event = yield undefined;

  is(request.transaction, event.target,
     "request.transaction should still be set");

  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(request.transaction, null, "request.transaction should be cleared");

  let transaction = db.transaction("foo", "readwrite");

  os = transaction.objectStore("foo");
  // Place a request to keep the transaction alive long enough for our
  // executeSoon.
  let requestComplete = false;

  let wasAbleToGrabObjectStoreOutsideOfCallback = false;
  let wasAbleToGrabIndexOutsideOfCallback = false;
  executeSoon(function() {
    // Worker Fails Here! Due to the thread switching of 'executeSoon()' the
    // transaction can commit and fire a 'complete' event before we continue.
    ok(!requestComplete, "Ordering is correct.");
    wasAbleToGrabObjectStoreOutsideOfCallback = !!transaction.objectStore("foo");
    wasAbleToGrabIndexOutsideOfCallback =
      !!transaction.objectStore("foo").index("bar");
  });

  request = os.add({});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  requestComplete = true;

  ok(wasAbleToGrabObjectStoreOutsideOfCallback,
     "Should be able to get objectStore");
  ok(wasAbleToGrabIndexOutsideOfCallback,
     "Should be able to get index");

  transaction.oncomplete = grabEventAndContinueHandler;
  yield undefined;

  try {
    transaction.objectStore("foo");
    ok(false, "Should have thrown!");
  }
  catch (e) {
    ok(e instanceof DOMException, "Got database exception.");
    is(e.name, "InvalidStateError", "Good error.");
    is(e.code, DOMException.INVALID_STATE_ERR, "Good error code.");
  }

  continueToNextStep();
  yield undefined;

  try {
    transaction.objectStore("foo");
    ok(false, "Should have thrown!");
  }
  catch (e) {
    ok(e instanceof DOMException, "Got database exception.");
    is(e.name, "InvalidStateError", "Good error.");
    is(e.code, DOMException.INVALID_STATE_ERR, "Good error code.");
  }

  finishTest();
  yield undefined;
}
