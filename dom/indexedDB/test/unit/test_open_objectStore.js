/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const objectStoreName = "Objects";

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  is(db.objectStoreNames.length, 0, "Bad objectStores list");

  let objectStore = db.createObjectStore(objectStoreName,
                                         { keyPath: "foo" });

  is(db.objectStoreNames.length, 1, "Bad objectStores list");
  is(db.objectStoreNames.item(0), objectStoreName, "Bad name");

  yield undefined;

  objectStore = db.transaction(objectStoreName).objectStore(objectStoreName);

  is(objectStore.name, objectStoreName, "Bad name");
  is(objectStore.keyPath, "foo", "Bad keyPath");
  if (objectStore.indexNames.length, 0, "Bad indexNames");

  finishTest();
}

