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
  db.createObjectStore("foo");
  yield;

  let transaction1 = db.transaction("foo");
  is(transaction1.readyState, IDBTransaction.INITIAL, "Correct readyState");

  let transaction2;

  if (this.window)
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  let thread = Components.classes["@mozilla.org/thread-manager;1"]
                         .getService()
                         .currentThread;

  let eventHasRun;

  thread.dispatch(function() {
    eventHasRun = true;

    is(transaction1.readyState, IDBTransaction.INITIAL,
       "Correct readyState");

    transaction2 = db.transaction("foo");
    is(transaction2.readyState, IDBTransaction.INITIAL,
       "Correct readyState");

  }, Components.interfaces.nsIThread.DISPATCH_NORMAL);

  while (!eventHasRun) {
    thread.processNextEvent(false);
  }

  is(transaction1.readyState, IDBTransaction.INITIAL, "Correct readyState");

  ok(transaction2, "Non-null transaction2");
  is(transaction2.readyState, IDBTransaction.DONE, "Correct readyState");

  continueToNextStep();
  yield;

  is(transaction1.readyState, IDBTransaction.DONE, "Correct readyState");

  finishTest();
  yield;
}
