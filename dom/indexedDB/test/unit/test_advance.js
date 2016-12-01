/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const dataCount = 30;

  let request = indexedDB.open(this.window ? window.location.pathname : "Splendid Test", 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  db.onerror = errorHandler;

  event.target.onsuccess = continueToNextStep;

  let objectStore = db.createObjectStore("", { keyPath: "key" });
  objectStore.createIndex("", "index");

  for (let i = 0; i < dataCount; i++) {
    objectStore.add({ key: i, index: i });
  }
  yield undefined;

  function getObjectStore() {
    return db.transaction("").objectStore("");
  }

  function getIndex() {
    return db.transaction("").objectStore("").index("");
  }

  let count = 0;

  getObjectStore().openCursor().onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      continueToNextStep();
    }
  };
  yield undefined;

  is(count, dataCount, "Saw all data");

  count = 0;

  getObjectStore().openCursor().onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.primaryKey, count, "Got correct object");
      if (count) {
        count++;
        cursor.continue();
      }
      else {
        count = 10;
        cursor.advance(10);
      }
    }
    else {
      continueToNextStep();
    }
  };
  yield undefined;

  is(count, dataCount, "Saw all data");

  count = 0;

  getIndex().openCursor().onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.primaryKey, count, "Got correct object");
      if (count) {
        count++;
        cursor.continue();
      }
      else {
        count = 10;
        cursor.advance(10);
      }
    }
    else {
      continueToNextStep();
    }
  };
  yield undefined;

  is(count, dataCount, "Saw all data");

  count = 0;

  getIndex().openKeyCursor().onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.primaryKey, count, "Got correct object");
      if (count) {
        count++;
        cursor.continue();
      }
      else {
        count = 10;
        cursor.advance(10);
      }
    }
    else {
      continueToNextStep();
    }
  };
  yield undefined;

  is(count, dataCount, "Saw all data");

  count = 0;

  getObjectStore().openCursor().onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.primaryKey, count, "Got correct object");
      if (count == 0) {
        cursor.advance(dataCount + 1);
      }
      else {
        ok(false, "Should never get here!");
        cursor.continue();
      }
    }
    else {
      continueToNextStep();
    }
  };
  yield undefined;

  is(count, 0, "Saw all data");

  count = dataCount - 1;

  getObjectStore().openCursor(null, "prev").onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.primaryKey, count, "Got correct object");
      count--;
      if (count == dataCount - 2) {
        cursor.advance(10);
        count -= 9;
      }
      else {
        cursor.continue();
      }
    }
    else {
      continueToNextStep();
    }
  };
  yield undefined;

  is(count, -1, "Saw all data");

  count = dataCount - 1;

  getObjectStore().openCursor(null, "prev").onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.primaryKey, count, "Got correct object");
      if (count == dataCount - 1) {
        cursor.advance(dataCount + 1);
      }
      else {
        ok(false, "Should never get here!");
        cursor.continue();
      }
    }
    else {
      continueToNextStep();
    }
  };
  yield undefined;

  is(count, dataCount - 1, "Saw all data");

  finishTest();
}
