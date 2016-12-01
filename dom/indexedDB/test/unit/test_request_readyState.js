/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";

  let request = indexedDB.open(name, 1);
  is(request.readyState, "pending", "Correct readyState");

  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  is(request.readyState, "done", "Correct readyState");

  let db = event.target.result;

  let objectStore = db.createObjectStore("foo");
  let key = 10;

  request = objectStore.add({}, key);
  is(request.readyState, "pending", "Correct readyState");

  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(request.readyState, "done", "Correct readyState");
  is(event.target.result, key, "Correct key");

  request = objectStore.get(key);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  is(request.readyState, "pending", "Correct readyState");
  event = yield undefined;

  ok(event.target.result, "Got something");
  is(request.readyState, "done", "Correct readyState");

  // Wait for success
  yield undefined;

  finishTest();
}
