/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

if (!this.window) {
  this.runTest = function() {
    todo(false, "Test disabled in xpcshell test suite for now");
    finishTest();
  }
}

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";

  // Needs to be enough to saturate the thread pool.
  const SYNC_REQUEST_COUNT = 25;

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  db.onerror = errorHandler;

  is(event.target.transaction.mode, "versionchange", "Correct mode");

  let objectStore = db.createObjectStore("foo", { autoIncrement: true });

  request = objectStore.add({});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  let key = event.target.result;
  ok(key, "Got a key");

  yield undefined;

  let continueReading = true;
  let readerCount = 0;
  let writerCount = 0;
  let callbackCount = 0;

  // Generate a bunch of reads right away without returning to the event
  // loop.
  info("Generating " + SYNC_REQUEST_COUNT + " readonly requests");

  for (let i = 0; i < SYNC_REQUEST_COUNT; i++) {
    readerCount++;
    let request = db.transaction("foo").objectStore("foo").get(key);
    request.onsuccess = function(event) {
      is(event.target.transaction.mode, "readonly", "Correct mode");
      callbackCount++;
    };
  }

  while (continueReading) {
    readerCount++;
    info("Generating additional readonly request (" + readerCount + ")");
    let request = db.transaction("foo").objectStore("foo").get(key);
    request.onsuccess = function(event) {
      callbackCount++;
      info("Received readonly request callback (" + callbackCount + ")");
      is(event.target.transaction.mode, "readonly", "Correct mode");
      if (callbackCount == SYNC_REQUEST_COUNT) {
        writerCount++;
        info("Generating 1 readwrite request with " + readerCount +
             " previous readonly requests");
        let request = db.transaction("foo", "readwrite")
                        .objectStore("foo")
                        .add({}, readerCount);
        request.onsuccess = function(event) {
          callbackCount++;
          info("Received readwrite request callback (" + callbackCount + ")");
          is(event.target.transaction.mode, "readwrite", "Correct mode");
          is(event.target.result, callbackCount,
             "write callback came before later reads");
        }
      }
      else if (callbackCount == SYNC_REQUEST_COUNT + 5) {
        continueReading = false;
      }
    };

    setTimeout(function() { testGenerator.next(); }, writerCount ? 1000 : 100);
    yield undefined;
  }

  while (callbackCount < (readerCount + writerCount)) {
    executeSoon(function() { testGenerator.next(); });
    yield undefined;
  }

  is(callbackCount, readerCount + writerCount, "All requests accounted for");

  finishTest();
  yield undefined;
}
