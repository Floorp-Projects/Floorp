/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const dbName1 = "upgrade_test";
  const dbName2 = "testing.foobar";
  const dbName3 = "xxxxxxx.xxxxxx";

  clearAllDatabases(continueToNextStepSync);
  yield undefined;

  installPackagedProfile("bug1056939_profile");

  let request = indexedDB.open(dbName1, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  is(event.type, "success", "Correct event type");

  request = indexedDB.open(dbName2, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.type, "success", "Got correct event type");

  request = indexedDB.open(dbName3, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.type, "success", "Got correct event type");

  clearAllDatabases(continueToNextStepSync);
  yield undefined;

  request = indexedDB.open(dbName3, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  event = yield undefined;

  is(event.type, "upgradeneeded", "Got correct event type");

  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.type, "success", "Got correct event type");

  resetAllDatabases(continueToNextStepSync);
  yield undefined;

  request = indexedDB.open(dbName3, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.type, "success", "Got correct event type");

  finishTest();
  yield undefined;
}
