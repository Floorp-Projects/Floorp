/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const objectStore =  { name: "Objects",
                         options: { keyPath: "id", autoIncrement: true } };

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;

  let db1 = event.target.result;

  is(db1.objectStoreNames.length, 0, "No objectStores in db1");

  db1.createObjectStore(objectStore.name, objectStore.options);

  continueToNextStep();
  yield undefined;

  request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  let db2 = event.target.result;

  ok(db1 !== db2, "Databases are not the same object");

  is(db1.objectStoreNames.length, 1, "1 objectStore in db1");
  is(db1.objectStoreNames.item(0), objectStore.name, "Correct name");

  is(db2.objectStoreNames.length, 1, "1 objectStore in db2");
  is(db2.objectStoreNames.item(0), objectStore.name, "Correct name");

  let objectStore1 = db1.transaction(objectStore.name)
                        .objectStore(objectStore.name);
  is(objectStore1.name, objectStore.name, "Same name");
  is(objectStore1.keyPath, objectStore.options.keyPath, "Same keyPath");

  let objectStore2 = db2.transaction(objectStore.name)
                        .objectStore(objectStore.name);

  ok(objectStore1 !== objectStore2, "Different objectStores");
  is(objectStore1.name, objectStore2.name, "Same name");
  is(objectStore1.keyPath, objectStore2.keyPath, "Same keyPath");

  finishTest();
}
