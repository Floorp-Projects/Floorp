/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps() {
  const dbName = this.window ?
                 window.location.pathname :
                 "test_objectStore_openKeyCursor";
  const dbVersion = 1;
  const objectStoreName = "foo";
  const keyCount = 100;

  let request = indexedDB.open(dbName, dbVersion);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;

  let event = yield undefined;

  info("Creating database");

  let db = event.target.result;
  let objectStore = db.createObjectStore(objectStoreName);
  for (let i = 0; i < keyCount; i++) {
    objectStore.add(true, i);
  }

  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  db = event.target.result;
  objectStore = db.transaction(objectStoreName, "readwrite")
                  .objectStore(objectStoreName);

  info("Getting all keys");
  objectStore.getAllKeys().onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  const allKeys = event.target.result;

  ok(Array.isArray(allKeys), "Got an array result");
  is(allKeys.length, keyCount, "Got correct array length");

  info("Opening normal key cursor");

  let seenKeys = [];
  objectStore.openKeyCursor().onsuccess = event => {
    let cursor = event.target.result;
    if (!cursor) {
      continueToNextStepSync();
      return;
    }

    is(cursor.source, objectStore, "Correct source");
    is(cursor.direction, "next", "Correct direction");

    let exception = null;
    try {
      cursor.update(10);
    } catch(e) {
      exception = e;
    }
    ok(!!exception, "update() throws for key cursor");

    exception = null;
    try {
      cursor.delete();
    } catch(e) {
      exception = e;
    }
    ok(!!exception, "delete() throws for key cursor");

    is(cursor.key, cursor.primaryKey, "key and primaryKey match");
    ok(!("value" in cursor), "No 'value' property on key cursor");

    seenKeys.push(cursor.key);
    cursor.continue();
  };
  yield undefined;

  is(seenKeys.length, allKeys.length, "Saw the right number of keys");

  let match = true;
  for (let i = 0; i < seenKeys.length; i++) {
    if (seenKeys[i] !== allKeys[i]) {
      match = false;
      break;
    }
  }
  ok(match, "All keys matched");

  info("Opening key cursor with keyRange");

  let keyRange = IDBKeyRange.bound(10, 20, false, true);

  seenKeys = [];
  objectStore.openKeyCursor(keyRange).onsuccess = event => {
    let cursor = event.target.result;
    if (!cursor) {
      continueToNextStepSync();
      return;
    }

    is(cursor.source, objectStore, "Correct source");
    is(cursor.direction, "next", "Correct direction");

    let exception = null;
    try {
      cursor.update(10);
    } catch(e) {
      exception = e;
    }
    ok(!!exception, "update() throws for key cursor");

    exception = null;
    try {
      cursor.delete();
    } catch(e) {
      exception = e;
    }
    ok(!!exception, "delete() throws for key cursor");

    is(cursor.key, cursor.primaryKey, "key and primaryKey match");
    ok(!("value" in cursor), "No 'value' property on key cursor");

    seenKeys.push(cursor.key);
    cursor.continue();
  };
  yield undefined;

  is(seenKeys.length, 10, "Saw the right number of keys");

  match = true;
  for (let i = 0; i < seenKeys.length; i++) {
    if (seenKeys[i] !== allKeys[i + 10]) {
      match = false;
      break;
    }
  }
  ok(match, "All keys matched");

  info("Opening key cursor with unmatched keyRange");

  keyRange = IDBKeyRange.bound(10000, 200000);

  seenKeys = [];
  objectStore.openKeyCursor(keyRange).onsuccess = event => {
    let cursor = event.target.result;
    if (!cursor) {
      continueToNextStepSync();
      return;
    }

    ok(false, "Shouldn't have any keys here");
    cursor.continue();
  };
  yield undefined;

  is(seenKeys.length, 0, "Saw the right number of keys");

  info("Opening reverse key cursor");

  seenKeys = [];
  objectStore.openKeyCursor(null, "prev").onsuccess = event => {
    let cursor = event.target.result;
    if (!cursor) {
      continueToNextStepSync();
      return;
    }

    is(cursor.source, objectStore, "Correct source");
    is(cursor.direction, "prev", "Correct direction");

    let exception = null;
    try {
      cursor.update(10);
    } catch(e) {
      exception = e;
    }
    ok(!!exception, "update() throws for key cursor");

    exception = null;
    try {
      cursor.delete();
    } catch(e) {
      exception = e;
    }
    ok(!!exception, "delete() throws for key cursor");

    is(cursor.key, cursor.primaryKey, "key and primaryKey match");
    ok(!("value" in cursor), "No 'value' property on key cursor");

    seenKeys.push(cursor.key);
    cursor.continue();
  };
  yield undefined;

  is(seenKeys.length, allKeys.length, "Saw the right number of keys");

  seenKeys.reverse();

  match = true;
  for (let i = 0; i < seenKeys.length; i++) {
    if (seenKeys[i] !== allKeys[i]) {
      match = false;
      break;
    }
  }
  ok(match, "All keys matched");

  info("Opening reverse key cursor with key range");

  keyRange = IDBKeyRange.bound(10, 20, false, true);

  seenKeys = [];
  objectStore.openKeyCursor(keyRange, "prev").onsuccess = event => {
    let cursor = event.target.result;
    if (!cursor) {
      continueToNextStepSync();
      return;
    }

    is(cursor.source, objectStore, "Correct source");
    is(cursor.direction, "prev", "Correct direction");

    let exception = null;
    try {
      cursor.update(10);
    } catch(e) {
      exception = e;
    }
    ok(!!exception, "update() throws for key cursor");

    exception = null;
    try {
      cursor.delete();
    } catch(e) {
      exception = e;
    }
    ok(!!exception, "delete() throws for key cursor");

    is(cursor.key, cursor.primaryKey, "key and primaryKey match");
    ok(!("value" in cursor), "No 'value' property on key cursor");

    seenKeys.push(cursor.key);
    cursor.continue();
  };
  yield undefined;

  is(seenKeys.length, 10, "Saw the right number of keys");

  seenKeys.reverse();

  match = true;
  for (let i = 0; i < 10; i++) {
    if (seenKeys[i] !== allKeys[i + 10]) {
      match = false;
      break;
    }
  }
  ok(match, "All keys matched");

  info("Opening reverse key cursor with unmatched key range");

  keyRange = IDBKeyRange.bound(10000, 200000);

  seenKeys = [];
  objectStore.openKeyCursor(keyRange, "prev").onsuccess = event => {
    let cursor = event.target.result;
    if (!cursor) {
      continueToNextStepSync();
      return;
    }

    ok(false, "Shouldn't have any keys here");
    cursor.continue();
  };
  yield undefined;

  is(seenKeys.length, 0, "Saw the right number of keys");

  info("Opening key cursor with advance");

  seenKeys = [];
  objectStore.openKeyCursor().onsuccess = event => {
    let cursor = event.target.result;
    if (!cursor) {
      continueToNextStepSync();
      return;
    }

    is(cursor.source, objectStore, "Correct source");
    is(cursor.direction, "next", "Correct direction");

    let exception = null;
    try {
      cursor.update(10);
    } catch(e) {
      exception = e;
    }
    ok(!!exception, "update() throws for key cursor");

    exception = null;
    try {
      cursor.delete();
    } catch(e) {
      exception = e;
    }
    ok(!!exception, "delete() throws for key cursor");

    is(cursor.key, cursor.primaryKey, "key and primaryKey match");
    ok(!("value" in cursor), "No 'value' property on key cursor");

    seenKeys.push(cursor.key);
    if (seenKeys.length == 1) {
      cursor.advance(10);
    } else {
      cursor.continue();
    }
  };
  yield undefined;

  is(seenKeys.length, allKeys.length - 9, "Saw the right number of keys");

  match = true;
  for (let i = 0, j = 0; i < seenKeys.length; i++) {
    if (seenKeys[i] !== allKeys[i + j]) {
      match = false;
      break;
    }
    if (i == 0) {
      j = 9;
    }
  }
  ok(match, "All keys matched");

  info("Opening key cursor with continue-to-key");

  seenKeys = [];
  objectStore.openKeyCursor().onsuccess = event => {
    let cursor = event.target.result;
    if (!cursor) {
      continueToNextStepSync();
      return;
    }

    is(cursor.source, objectStore, "Correct source");
    is(cursor.direction, "next", "Correct direction");

    let exception = null;
    try {
      cursor.update(10);
    } catch(e) {
      exception = e;
    }
    ok(!!exception, "update() throws for key cursor");

    exception = null;
    try {
      cursor.delete();
    } catch(e) {
      exception = e;
    }
    ok(!!exception, "delete() throws for key cursor");

    is(cursor.key, cursor.primaryKey, "key and primaryKey match");
    ok(!("value" in cursor), "No 'value' property on key cursor");

    seenKeys.push(cursor.key);

    if (seenKeys.length == 1) {
      cursor.continue(10);
    } else {
      cursor.continue();
    }
  };
  yield undefined;

  is(seenKeys.length, allKeys.length - 9, "Saw the right number of keys");

  match = true;
  for (let i = 0, j = 0; i < seenKeys.length; i++) {
    if (seenKeys[i] !== allKeys[i + j]) {
      match = false;
      break;
    }
    if (i == 0) {
      j = 9;
    }
  }
  ok(match, "All keys matched");

  finishTest();
  yield undefined;
}
