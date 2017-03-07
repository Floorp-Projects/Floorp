/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const url = "moz-extension://8ea6d31b-917c-431f-a204-15b95e904d4f";
  const dbName = "Hello.";
  const dbVersion = 1;

  clearAllDatabases(continueToNextStepSync);
  yield;

  // The origin directory contained in the package is:
  // "moz-extension+++8ea6d31b-917c-431f-a204-15b95e904d4f^addonId=indexedDB-test%40kmaglione.mozilla.com"
  installPackagedProfile("obsoleteOriginAttributes_profile");

  let request =
    indexedDB.openForPrincipal(getPrincipal(url), dbName, dbVersion);
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield;

  is(event.type, "success", "Correct event type");

  resetAllDatabases(continueToNextStepSync);
  yield;

  request = indexedDB.openForPrincipal(getPrincipal(url), dbName, dbVersion);
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.type, "success", "Correct event type");

  finishTest();
}

