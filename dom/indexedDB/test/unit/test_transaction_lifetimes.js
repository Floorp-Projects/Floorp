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

  event.target.onsuccess = continueToNextStep;

  db.createObjectStore("foo", { autoIncrement: true });
  yield;

  let transaction = db.transaction("foo");
  continueToNextStep();
  yield;

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
  yield;
}
