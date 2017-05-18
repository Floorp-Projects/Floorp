/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "test_setVersion_throw";

  // This test requires two databases. The first needs to be a low version
  // number that gets closed when a second higher version number database is
  // created. Then the upgradeneeded event for the second database throws an
  // exception and triggers an abort/close.

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  request.onupgradeneeded = function(event) {
    info("Got upgradeneeded event for db 1");
  };
  let event = yield undefined;

  is(event.type, "success", "Got success event for db 1");

  let db = event.target.result;
  db.onversionchange = function(event) {
    info("Got versionchange event for db 1");
    event.target.close();
  }

  executeSoon(continueToNextStepSync);
  yield undefined;

  request = indexedDB.open(name, 2);
  request.onerror = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  request.onupgradeneeded = function(event) {
    info("Got upgradeneeded event for db 2");
    expectUncaughtException(true);
    // eslint-disable-next-line no-undef
    trigger_js_exception_by_calling_a_nonexistent_function();
  };
  event = yield undefined;

  event.preventDefault();

  is(event.type, "error", "Got an error event for db 2");
  ok(event.target.error instanceof DOMError, "Request has a DOMError");
  is(event.target.error.name, "AbortError", "Request has AbortError");

  finishTest();
}
