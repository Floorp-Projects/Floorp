/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var disableWorkerTest = "Need to implement a gc() function for worker tests";

var testGenerator = testSteps();

function testSteps()
{
  const dbName = ("window" in this) ? window.location.pathname : "test";
  const dbVersion = 1;
  const objectStoreName = "foo";
  const data = 0;

  let req = indexedDB.open(dbName, dbVersion);
  req.onerror = errorHandler;
  req.onupgradeneeded = grabEventAndContinueHandler;
  req.onsuccess = grabEventAndContinueHandler;

  let event = yield undefined;

  is(event.type, "upgradeneeded", "Got upgradeneeded event");

  let db = event.target.result;

  let objectStore =
    db.createObjectStore(objectStoreName, { autoIncrement: true });
  objectStore.add(data);

  event = yield undefined;

  is(event.type, "success", "Got success event for open");

  objectStore = db.transaction(objectStoreName).objectStore(objectStoreName);

  objectStore.openCursor().onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.type, "success", "Got success event for openCursor");

  let cursor = event.target.result;
  is(cursor.value, data, "Got correct cursor value");

  objectStore.get(cursor.key).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, data, "Got correct get value");

  info("Collecting garbage");

  gc();

  info("Done collecting garbage");

  cursor.continue();
  event = yield undefined;

  is(event.target.result, null, "No more entries");

  finishTest();
  yield undefined;
}
