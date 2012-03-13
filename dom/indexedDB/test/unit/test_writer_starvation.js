/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const description = "My Test Database";

  let request = mozIndexedDB.open(name, 1, description);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield;

  let db = event.target.result;

  is(event.target.transaction.mode, "versionchange", "Correct mode");

  let objectStore = db.createObjectStore("foo", { autoIncrement: true });

  request = objectStore.add({});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  let key = event.target.result;
  ok(key, "Got a key");

  yield;

  let continueReading = true;
  let readerCount = 0;
  let callbackCount = 0;
  let finalCallbackCount = 0;

  // Generate a bunch of reads right away without returning to the event
  // loop.
  for (let i = 0; i < 20; i++) {
    readerCount++;
    request = db.transaction("foo").objectStore("foo").get(key);
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      callbackCount++;
    };
  }

  while (continueReading) {
    readerCount++;
    request = db.transaction("foo").objectStore("foo").get(key);
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      is(event.target.transaction.mode, "readonly", "Correct mode");
      callbackCount++;
      if (callbackCount == 100) {
        request = db.transaction("foo", "readwrite")
                    .objectStore("foo")
                    .add({}, readerCount);
        request.onerror = errorHandler;
        request.onsuccess = function(event) {
          continueReading = false;
          finalCallbackCount = callbackCount;
          is(event.target.result, callbackCount,
             "write callback came before later reads");
        }
      }
    };

    executeSoon(function() { testGenerator.next(); });
    yield;
  }

  while (callbackCount < readerCount) {
    executeSoon(function() { testGenerator.next(); });
    yield;
  }

  is(callbackCount, readerCount, "All requests accounted for");
  ok(callbackCount > finalCallbackCount, "More readers after writer");

  finishTest();
  yield;
}

if (this.window)
  SimpleTest.requestLongerTimeout(5); // see bug 580875

