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

  let eventHasRun;

  let tm = SpecialPowers.Services ? SpecialPowers.Services.tm : Services.tm;

  tm.dispatchToMainThread(function() {
    eventHasRun = true;

    transaction2 = db.transaction("foo");
  });

  tm.spinEventLoopUntil(() => eventHasRun);

  ok(transaction2, "Non-null transaction2");

  continueToNextStep();
  yield undefined;

  finishTest();
}
