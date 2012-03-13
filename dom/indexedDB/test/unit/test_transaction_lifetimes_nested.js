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

  let transaction2;

  if (this.window)
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  let thread = Components.classes["@mozilla.org/thread-manager;1"]
                         .getService()
                         .currentThread;

  let eventHasRun;

  thread.dispatch(function() {
    eventHasRun = true;

    transaction2 = db.transaction("foo");
  }, Components.interfaces.nsIThread.DISPATCH_NORMAL);

  while (!eventHasRun) {
    thread.processNextEvent(false);
  }

  ok(transaction2, "Non-null transaction2");

  continueToNextStep();
  yield;

  finishTest();
  yield;
}
