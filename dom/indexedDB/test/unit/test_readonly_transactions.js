/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const osName = "foo";

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  is(db.objectStoreNames.length, 0, "Correct objectStoreNames list");

  db.createObjectStore(osName, { autoIncrement: "true" });

  yield undefined;

  let key1, key2;

  request = db.transaction([osName], "readwrite")
              .objectStore(osName)
              .add({});
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    is(event.target.transaction.mode, "readwrite", "Correct mode");
    key1 = event.target.result;
    testGenerator.next();
  }
  yield undefined;

  request = db.transaction(osName, "readwrite").objectStore(osName).add({});
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    is(event.target.transaction.mode, "readwrite", "Correct mode");
    key2 = event.target.result;
    testGenerator.next();
  }
  yield undefined;

  request = db.transaction([osName], "readwrite")
              .objectStore(osName)
              .put({}, key1);
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    is(event.target.transaction.mode, "readwrite", "Correct mode");
    testGenerator.next();
  }
  yield undefined;

  request = db.transaction(osName, "readwrite")
              .objectStore(osName)
              .put({}, key2);
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    is(event.target.transaction.mode, "readwrite", "Correct mode");
    testGenerator.next();
  }
  yield undefined;

  request = db.transaction([osName], "readwrite")
              .objectStore(osName)
              .put({}, key1);
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    is(event.target.transaction.mode, "readwrite", "Correct mode");
    testGenerator.next();
  }
  yield undefined;

  request = db.transaction(osName, "readwrite")
              .objectStore(osName)
              .put({}, key1);
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    is(event.target.transaction.mode, "readwrite", "Correct mode");
    testGenerator.next();
  }
  yield undefined;

  request = db.transaction([osName], "readwrite")
              .objectStore(osName)
              .delete(key1);
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    is(event.target.transaction.mode, "readwrite", "Correct mode");
    testGenerator.next();
  }
  yield undefined;

  request = db.transaction(osName, "readwrite")
              .objectStore(osName)
              .delete(key2);
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    is(event.target.transaction.mode, "readwrite", "Correct mode");
    testGenerator.next();
  }
  yield undefined;

  try {
    request = db.transaction([osName]).objectStore(osName).add({});
    ok(false, "Adding to a readonly transaction should fail!");
  }
  catch (e) {
    ok(true, "Adding to a readonly transaction failed");
  }

  try {
    request = db.transaction(osName).objectStore(osName).add({});
    ok(false, "Adding to a readonly transaction should fail!");
  }
  catch (e) {
    ok(true, "Adding to a readonly transaction failed");
  }

  try {
    request = db.transaction([osName]).objectStore(osName).put({});
    ok(false, "Adding or modifying a readonly transaction should fail!");
  }
  catch (e) {
    ok(true, "Adding or modifying a readonly transaction failed");
  }

  try {
    request = db.transaction(osName).objectStore(osName).put({});
    ok(false, "Adding or modifying a readonly transaction should fail!");
  }
  catch (e) {
    ok(true, "Adding or modifying a readonly transaction failed");
  }

  try {
    request = db.transaction([osName]).objectStore(osName).put({}, key1);
    ok(false, "Modifying a readonly transaction should fail!");
  }
  catch (e) {
    ok(true, "Modifying a readonly transaction failed");
  }

  try {
    request = db.transaction(osName).objectStore(osName).put({}, key1);
    ok(false, "Modifying a readonly transaction should fail!");
  }
  catch (e) {
    ok(true, "Modifying a readonly transaction failed");
  }

  try {
    request = db.transaction([osName]).objectStore(osName).delete(key1);
    ok(false, "Removing from a readonly transaction should fail!");
  }
  catch (e) {
    ok(true, "Removing from a readonly transaction failed");
  }

  try {
    request = db.transaction(osName).objectStore(osName).delete(key2);
    ok(false, "Removing from a readonly transaction should fail!");
  }
  catch (e) {
    ok(true, "Removing from a readonly transaction failed");
  }

  finishTest();
  yield undefined;
}
