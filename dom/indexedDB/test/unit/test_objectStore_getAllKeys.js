/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps() {
  const dbName = this.window ?
                 window.location.pathname :
                 "test_objectStore_getAllKeys";
  const dbVersion = 1;
  const objectStoreName = "foo";
  const keyCount = 200;

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
  objectStore = db.transaction(objectStoreName).objectStore(objectStoreName);

  info("Getting all keys");
  objectStore.getAllKeys().onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  ok(Array.isArray(event.target.result), "Got an array result");
  is(event.target.result.length, keyCount, "Got correct array length");

  let match = true;
  for (let i = 0; i < keyCount; i++) {
    if (event.target.result[i] != i) {
      match = false;
      break;
    }
  }
  ok(match, "Got correct keys");

  info("Getting all keys with key range");
  let keyRange = IDBKeyRange.bound(10, 20, false, true);
  objectStore.getAllKeys(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  ok(Array.isArray(event.target.result), "Got an array result");
  is(event.target.result.length, 10, "Got correct array length");

  match = true;
  for (let i = 10; i < 20; i++) {
    if (event.target.result[i - 10] != i) {
      match = false;
      break;
    }
  }
  ok(match, "Got correct keys");

  info("Getting all keys with unmatched key range");
  keyRange = IDBKeyRange.bound(10000, 200000);
  objectStore.getAllKeys(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  ok(Array.isArray(event.target.result), "Got an array result");
  is(event.target.result.length, 0, "Got correct array length");

  info("Getting all keys with limit");
  objectStore.getAllKeys(null, 5).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  ok(Array.isArray(event.target.result), "Got an array result");
  is(event.target.result.length, 5, "Got correct array length");

  match = true;
  for (let i = 0; i < 5; i++) {
    if (event.target.result[i] != i) {
      match = false;
      break;
    }
  }
  ok(match, "Got correct keys");

  info("Getting all keys with key range and limit");
  keyRange = IDBKeyRange.bound(10, 20, false, true);
  objectStore.getAllKeys(keyRange, 5).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  ok(Array.isArray(event.target.result), "Got an array result");
  is(event.target.result.length, 5, "Got correct array length");

  match = true;
  for (let i = 10; i < 15; i++) {
    if (event.target.result[i - 10] != i) {
      match = false;
      break;
    }
  }
  ok(match, "Got correct keys");

  info("Getting all keys with unmatched key range and limit");
  keyRange = IDBKeyRange.bound(10000, 200000);
  objectStore.getAllKeys(keyRange, 5).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  ok(Array.isArray(event.target.result), "Got an array result");
  is(event.target.result.length, 0, "Got correct array length");

  finishTest();
}
