/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var disableWorkerTest = "Need a way to set temporary prefs from a worker";

var testGenerator = testSteps();

function* testSteps()
{
  const name =
    this.window ? window.location.pathname : "test_readwriteflush_disabled.js";

  info("Resetting experimental pref");

  if (this.window) {
    SpecialPowers.pushPrefEnv(
      {
        "set": [
          ["dom.indexedDB.experimental", false]
        ]
      },
      continueToNextStep
    );
    yield undefined;
  } else {
    resetExperimental();
  }

  info("Opening database");

  let request = indexedDB.open(name);
  request.onerror = errorHandler;
  request.onupgradeneeded = continueToNextStepSync;
  request.onsuccess = unexpectedSuccessHandler;

  yield undefined;

  // upgradeneeded
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = continueToNextStepSync;

  info("Creating objectStore");

  request.result.createObjectStore(name);

  yield undefined;

  // success
  let db = request.result;

  info("Attempting to create a 'readwriteflush' transaction");

  let exception;

  try {
    let transaction = db.transaction(name, "readwriteflush");
  } catch (e) {
    exception = e;
  }

  ok(exception, "'readwriteflush' transaction threw");
  ok(exception instanceof Error, "exception is an Error object");
  is(exception.message,
     "Argument 2 of IDBDatabase.transaction 'readwriteflush' is not a valid " +
     "value for enumeration IDBTransactionMode.",
     "exception has the correct message");

  finishTest();
}
