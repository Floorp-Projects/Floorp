/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const objectStoreName = "Objects";

  let testString = { key: 0, value: "testString" };
  let testInt = { key: 1, value: 1002 };

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;

  let objectStore = db.createObjectStore(objectStoreName,
                                         { autoIncrement: 0 });

  request = objectStore.add(testString.value, testString.key);
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    is(event.target.result, testString.key, "Got the right key");
    request = objectStore.get(testString.key);
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      is(event.target.result, testString.value, "Got the right value");
    };
  };

  request = objectStore.add(testInt.value, testInt.key);
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    is(event.target.result, testInt.key, "Got the right key");
    request = objectStore.get(testInt.key);
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      is(event.target.result, testInt.value, "Got the right value");
      finishTest();
    };
  }

  yield undefined;
}

