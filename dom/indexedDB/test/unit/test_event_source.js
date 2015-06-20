/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const objectStoreName = "Objects";

  var request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  var event = yield undefined;

  is(event.target.source, null, "correct event.target.source");

  var db = event.target.result;
  var objectStore = db.createObjectStore(objectStoreName,
                                         { autoIncrement: true });
  request = objectStore.add({});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  ok(event.target.source === objectStore, "correct event.source");

  // Wait for success
  yield undefined;

  finishTest();
  yield undefined;
}
