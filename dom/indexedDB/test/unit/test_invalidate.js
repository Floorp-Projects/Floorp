/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let testGenerator = testSteps();

function testSteps()
{
  const databaseName =
    ("window" in this) ? window.location.pathname : "Test";

  let dbCount = 0;

  // Test invalidating during a versionchange transaction.
  info("Opening database " + ++dbCount);

  let request = indexedDB.open(databaseName, dbCount);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;

  is(event.type, "upgradeneeded", "Upgrading database " + dbCount);

  request.onupgradeneeded = unexpectedSuccessHandler;

  let objStore =
    request.result.createObjectStore("foo", { autoIncrement: true });
  objStore.createIndex("fooIndex", "fooIndex", { unique: true });
  objStore.put({ foo: 1 });
  objStore.get(1);
  objStore.count();
  objStore.openCursor();
  objStore.delete(1);

  info("Invalidating database " + dbCount);

  clearAllDatabases(continueToNextStepSync);

  objStore =  request.result.createObjectStore("bar");
  objStore.createIndex("barIndex", "barIndex", { multiEntry: true });
  objStore.put({ bar: 1, barIndex: [ 0, 1 ] }, 10);
  objStore.get(10);
  objStore.count();
  objStore.openCursor();
  objStore.delete(10);

  yield undefined;

  executeSoon(continueToNextStepSync);
  yield undefined;

  // Test invalidating after the complete event of a versionchange transaction.
  info("Opening database " + ++dbCount);

  request = indexedDB.open(databaseName, dbCount);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  event = yield undefined;

  is(event.type, "upgradeneeded", "Upgrading database " + dbCount);

  request.onupgradeneeded = unexpectedSuccessHandler;

  request.transaction.oncomplete = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.type, "complete",
     "Got complete event for versionchange transaction on database " + dbCount);

  info("Invalidating database " + dbCount);

  clearAllDatabases(continueToNextStepSync);
  yield undefined;

  executeSoon(continueToNextStepSync);

  finishTest();
  yield undefined;
}
