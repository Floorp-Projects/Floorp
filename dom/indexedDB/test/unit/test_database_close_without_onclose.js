/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname :
                             "test_database_close_without_onclose.js";

  const checkpointSleepTimeSec = 10;

  let openRequest = indexedDB.open(name, 1);
  openRequest.onerror = errorHandler;
  openRequest.onsuccess = unexpectedSuccessHandler;
  openRequest.onupgradeneeded = grabEventAndContinueHandler;

  ok(openRequest instanceof IDBOpenDBRequest, "Expect an IDBOpenDBRequest");

  let event = yield undefined;

  is(event.type, "upgradeneeded", "Expect an upgradeneeded event");
  ok(event instanceof IDBVersionChangeEvent, "Expect a versionchange event");

  let db = event.target.result;
  db.createObjectStore("store");

  openRequest.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  is(event.type, "success", "Expect a success event");
  is(event.target, openRequest, "Event has right target");
  ok(event.target.result instanceof IDBDatabase, "Result should be a database");
  is(db.objectStoreNames.length, 1, "Expect an objectStore here");

  db.onclose = errorHandler;

  db.close();
  setTimeout(continueToNextStepSync, checkpointSleepTimeSec * 1000);
  yield undefined;

  ok(true, "The close event should not be fired after closed normally!");

  finishTest();
  yield undefined;
}
