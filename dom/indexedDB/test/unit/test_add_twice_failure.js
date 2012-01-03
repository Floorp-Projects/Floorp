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
  let event = yield;

  let db = request.result;

  ok(event.target === request, "Good event target");

  let objectStore = db.createObjectStore("foo", { keyPath: null });
  let key = 10;

  request = objectStore.add({}, key);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(request.result, key, "Correct key");

  request = objectStore.add({}, key);
  request.onerror = new ExpectError(IDBDatabaseException.CONSTRAINT_ERR);
  request.onsuccess = unexpectedSuccessHandler;
  yield;

  finishTest();
  yield;
}

