/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";

  let request = indexedDB.open(name, 1);
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
  request.addEventListener("error", new ExpectError("ConstraintError", true));
  request.onsuccess = unexpectedSuccessHandler;
  yield;

  finishTest();
  yield;
}

