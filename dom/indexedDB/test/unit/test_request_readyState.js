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
  is(request.readyState, "pending", "Correct readyState");

  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield;

  is(request.readyState, "done", "Correct readyState");

  let db = event.target.result;

  let objectStore = db.createObjectStore("foo");
  let key = 10;

  request = objectStore.add({}, key);
  is(request.readyState, "pending", "Correct readyState");

  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(request.readyState, "done", "Correct readyState");
  is(event.target.result, key, "Correct key");

  request = objectStore.get(key);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  is(request.readyState, "pending", "Correct readyState");
  event = yield;

  ok(event.target.result, "Got something");
  is(request.readyState, "done", "Correct readyState");

  finishTest();
  yield;
}
