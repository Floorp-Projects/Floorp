/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const description = "My Test Database";
  const objectStoreName = "Objects";

  var request = mozIndexedDB.open(name, 1, description);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  var event = yield;

  is(event.target.source, null, "correct event.target.source");

  var db = event.target.result;
  var objectStore = db.createObjectStore(objectStoreName,
                                         { autoIncrement: true });
  request = objectStore.add({});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  ok(event.target.source === objectStore, "correct event.source");

  finishTest();
  yield;
}
