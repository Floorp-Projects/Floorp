/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const description = "My Test Database";
  const entryCount = 1000;

  let request = indexedDB.open(name, 1, description);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield;

  let db = request.result;

  event.target.onsuccess = continueToNextStep;

  let objectStore = db.createObjectStore("foo", { autoIncrement: true });

  let firstKey;
  for (let i = 0; i < entryCount; i++) {
    request = objectStore.add({});
    request.onerror = errorHandler;
    if (!i) {
      request.onsuccess = function(event) {
        firstKey = event.target.result;
      };
    }
  }
  yield;

  isnot(firstKey, undefined, "got first key");

  let seenEntryCount = 0;

  request = db.transaction("foo").objectStore("foo").openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      seenEntryCount++;
      cursor.continue();
    }
    else {
      continueToNextStep();
    }
  }
  yield;

  is(seenEntryCount, entryCount, "Correct entry count");

  try {
    db.transaction("foo").objectStore("foo").clear();
    ok(false, "clear should throw on READ_ONLY transactions");
  }
  catch (e) {
    ok(true, "clear should throw on READ_ONLY transactions");
  }

  request = db.transaction("foo", "readwrite").objectStore("foo").clear();
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  ok(event.target.result === undefined, "Correct event.target.result");
  ok(request.result === undefined, "Correct request.result");
  ok(request === event.target, "Correct event.target");

  request = db.transaction("foo").objectStore("foo").openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    let cursor = request.result;
    if (cursor) {
      ok(false, "Shouldn't have any entries");
    }
    continueToNextStep();
  }
  yield;

  request = db.transaction("foo", "readwrite").objectStore("foo").add({});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  isnot(event.target.result, firstKey, "Got a different key");

  finishTest();
  yield;
}
