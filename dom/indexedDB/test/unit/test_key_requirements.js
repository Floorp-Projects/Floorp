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

  let db = event.target.result;
  db.addEventListener("error", function(event) {
    event.preventDefault();
  }, false);

  let objectStore = db.createObjectStore("foo", { autoIncrement: true });

  request = objectStore.add({});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  let key1 = event.target.result;

  request = objectStore.put({}, key1);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, key1, "put gave the same key back");

  let key2 = 10;

  request = objectStore.put({}, key2);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, key2, "put gave the same key back");

  key2 = 100;

  request = objectStore.add({}, key2);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, key2, "put gave the same key back");

  try {
    objectStore.put({});
    ok(true, "put with no key should not throw with autoIncrement!");
  }
  catch (e) {
    ok(false, "put with no key threw with autoIncrement");
  }

  try {
    objectStore.put({});
    ok(true, "put with no key should not throw with autoIncrement!");
  }
  catch (e) {
    ok(false, "put with no key threw with autoIncrement");
  }

  try {
    objectStore.delete();
    ok(false, "remove with no key should throw!");
  }
  catch (e) {
    ok(true, "remove with no key threw");
  }

  objectStore = db.createObjectStore("bar");

  try {
    objectStore.add({});
    ok(false, "add with no key should throw!");
  }
  catch (e) {
    ok(true, "add with no key threw");
  }

  try {
    objectStore.put({});
    ok(false, "put with no key should throw!");
  }
  catch (e) {
    ok(true, "put with no key threw");
  }

  try {
    objectStore.put({});
    ok(false, "put with no key should throw!");
  }
  catch (e) {
    ok(true, "put with no key threw");
  }

  try {
    objectStore.delete();
    ok(false, "remove with no key should throw!");
  }
  catch (e) {
    ok(true, "remove with no key threw");
  }

  objectStore = db.createObjectStore("baz", { keyPath: "id" });

  try {
    objectStore.add({});
    ok(false, "add with no key should throw!");
  }
  catch (e) {
    ok(true, "add with no key threw");
  }

  try {
    objectStore.add({id:5}, 5);
    ok(false, "add with inline key and passed key should throw!");
  }
  catch (e) {
    ok(true, "add with inline key and passed key threw");
  }

  try {
    objectStore.put({});
    ok(false, "put with no key should throw!");
  }
  catch (e) {
    ok(true, "put with no key threw");
  }

  try {
    objectStore.put({});
    ok(false, "put with no key should throw!");
  }
  catch (e) {
    ok(true, "put with no key threw");
  }

  try {
    objectStore.delete();
    ok(false, "remove with no key should throw!");
  }
  catch (e) {
    ok(true, "remove with no key threw");
  }

  key1 = 10;

  request = objectStore.add({id:key1});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, key1, "add gave back the same key");

  request = objectStore.put({id:10});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, key1, "put gave back the same key");

  request = objectStore.put({id:10});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, key1, "put gave back the same key");

  request = objectStore.add({id:10});
  request.onerror = new ExpectError("ConstraintError", true);
  request.onsuccess = unexpectedSuccessHandler;
  event = yield;

  try {
    objectStore.add({}, null);
    ok(false, "add with null key should throw!");
  }
  catch (e) {
    ok(true, "add with null key threw");
  }

  try {
    objectStore.put({}, null);
    ok(false, "put with null key should throw!");
  }
  catch (e) {
    ok(true, "put with null key threw");
  }

  try {
    objectStore.put({}, null);
    ok(false, "put with null key should throw!");
  }
  catch (e) {
    ok(true, "put with null key threw");
  }

  try {
    objectStore.delete({}, null);
    ok(false, "remove with null key should throw!");
  }
  catch (e) {
    ok(true, "remove with null key threw");
  }

  objectStore = db.createObjectStore("bazing", { keyPath: "id",
                                                 autoIncrement: true });

  request = objectStore.add({});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  key1 = event.target.result;

  request = objectStore.put({id:key1});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, key1, "put gave the same key back");

  key2 = 10;

  request = objectStore.put({id:key2});
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, key2, "put gave the same key back");

  try {
    objectStore.put({});
    ok(true, "put with no key should not throw with autoIncrement!");
  }
  catch (e) {
    ok(false, "put with no key threw with autoIncrement");
  }

  try {
    objectStore.put({});
    ok(true, "put with no key should not throw with autoIncrement!");
  }
  catch (e) {
    ok(false, "put with no key threw with autoIncrement");
  }

  try {
    objectStore.delete();
    ok(false, "remove with no key should throw!");
  }
  catch (e) {
    ok(true, "remove with no key threw");
  }

  try {
    objectStore.add({id:5}, 5);
    ok(false, "add with inline key and passed key should throw!");
  }
  catch (e) {
    ok(true, "add with inline key and passed key threw");
  }

  request = objectStore.delete(key2);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield;

  finishTest();
  yield;
}