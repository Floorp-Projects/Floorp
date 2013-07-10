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

  event.target.onsuccess = continueToNextStep;

  let objectStore = db.createObjectStore("foo");
  objectStore.add({}, 1).onerror = errorHandler;

  yield undefined;

  objectStore = db.transaction("foo").objectStore("foo");

  let transaction = objectStore.transaction;
  transaction.oncomplete = unexpectedSuccessHandler;
  transaction.onabort = grabEventAndContinueHandler;

  let sawError = false;

  request = objectStore.get(1);
  request.onsuccess = unexpectedSuccessHandler;
  request.onerror = function(event) {
    is(event.target.error.name, "AbortError", "Good error");
    sawError = true;
    event.preventDefault();
  }

  transaction.abort();

  event = yield undefined;

  is(event.type, "abort", "Got abort event");
  is(sawError, true, "Saw get() error");
  if (this.window) {
    // Make sure the success event isn't queued somehow.
    let comp = SpecialPowers.wrap(Components);
    let thread = comp.classes["@mozilla.org/thread-manager;1"]
                     .getService(comp.interfaces.nsIThreadManager)
                     .currentThread;
    while (thread.hasPendingEvents()) {
      thread.processNextEvent(false);
    }
  }

  finishTest();
  yield undefined;
}

