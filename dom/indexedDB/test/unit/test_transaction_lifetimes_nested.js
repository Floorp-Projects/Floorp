/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var disableWorkerTest = "This test uses SpecialPowers";

var testGenerator = testSteps();

function* testSteps()
{
  let request = indexedDB.open(this.window ? window.location.pathname : "Splendid Test", 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  db.onerror = errorHandler;

  event.target.onsuccess = continueToNextStep;
  db.createObjectStore("foo");
  yield undefined;

  db.transaction("foo");

  let transaction2;

  let comp = this.window ? SpecialPowers.wrap(Components) : Components;
  let tm = comp.classes["@mozilla.org/thread-manager;1"]
               .getService(comp.interfaces.nsIThreadManager);
  let thread = tm.currentThread;

  let eventHasRun;

  thread.dispatch(function() {
    eventHasRun = true;

    transaction2 = db.transaction("foo");
  }, Components.interfaces.nsIThread.DISPATCH_NORMAL);

  tm.spinEventLoopUntil(() => eventHasRun);

  ok(transaction2, "Non-null transaction2");

  continueToNextStep();
  yield undefined;

  finishTest();
}
