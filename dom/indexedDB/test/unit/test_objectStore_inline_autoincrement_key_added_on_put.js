/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const IDBObjectStore = Components.interfaces.nsIIDBObjectStore;
  const name = this.window ? window.location.pathname : "Splendid Test";

  var request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  var event = yield;

  var db = event.target.result;

  var test = {
    name: "inline key; key generator",
    autoIncrement: true,
    storedObject: {name: "Lincoln"},
    keyName: "id",
  };

  let objectStore = db.createObjectStore(test.name,
                                         { keyPath: test.keyName,
                                           autoIncrement: test.autoIncrement });

  request = objectStore.add(test.storedObject);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  let id = event.target.result;
  request = objectStore.get(id);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  // Sanity check!
  is(event.target.result.name, test.storedObject.name,
     "The correct object was stored.");

  // Ensure that the id was also stored on the object.
  is(event.target.result.id, id, "The object had the id stored on it.");

  finishTest();
  yield;
}

