/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const data = { key: 5, index: 10 };

  let request = mozIndexedDB.open(this.window ? window.location.pathname : "Splendid Test", 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield;

  let db = event.target.result;

  ok(db instanceof IDBDatabase, "Got a real database");

  db.onerror = errorHandler;

  let objectStore = db.createObjectStore("foo", { keyPath: "key",
                                                  autoIncrement: true });
  let index = objectStore.createIndex("foo", "index");

  event.target.onsuccess = continueToNextStep;
  yield;

  objectStore = db.transaction("foo", "readwrite")
                  .objectStore("foo");
  request = objectStore.add(data);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  let key;
  executeSoon(function() {
    key = request.result;
    continueToNextStep();
  });
  yield;

  is(key, data.key, "Got the right key");

  objectStore = db.transaction("foo").objectStore("foo");
  objectStore.get(data.key).onsuccess = grabEventAndContinueHandler;
  event = yield;

  let obj;
  executeSoon(function() {
    obj = event.target.result;
    continueToNextStep();
  });
  yield;

  is(obj.key, data.key, "Got the right key");
  is(obj.index, data.index, "Got the right property value");

  objectStore = db.transaction("foo", "readwrite")
                  .objectStore("foo");
  request = objectStore.delete(data.key);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  key = undefined;
  executeSoon(function() {
    key = request.result;
    continueToNextStep();
  }, 0);
  yield;

  ok(key === undefined, "Got the right value");

  finishTest();
  yield;
}
