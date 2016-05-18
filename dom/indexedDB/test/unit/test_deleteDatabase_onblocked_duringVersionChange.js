/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const dbVersion = 10;

  let openRequest = indexedDB.open(name, dbVersion);
  openRequest.onerror = errorHandler;
  openRequest.onblocked = errorHandler;
  openRequest.onsuccess = unexpectedSuccessHandler;
  openRequest.onupgradeneeded = grabEventAndContinueHandler;

  let event = yield undefined;

  is(event.type, "upgradeneeded", "Expect an upgradeneeded event");
  ok(event instanceof IDBVersionChangeEvent, "Expect a versionchange event");

  let db = event.target.result;
  db.onversionchange = errorHandler;
  db.createObjectStore("stuff");

  let deletingRequest = indexedDB.deleteDatabase(name);
  deletingRequest.onerror = errorHandler;
  deletingRequest.onsuccess = errorHandler;
  deletingRequest.onblocked = errorHandler;

  openRequest.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  is(event.type, "success", "Expect a success event");
  is(event.target, openRequest, "Event has right target");
  ok(event.target.result instanceof IDBDatabase, "Result should be a database");
  is(db.objectStoreNames.length, 1, "Expect an objectStore here");

  db.onversionchange = grabEventAndContinueHandler;

  event = yield undefined;

  is(event.type, "versionchange", "Expect an versionchange event");
  is(event.target, db, "Event has right target");
  ok(event instanceof IDBVersionChangeEvent, "Expect a versionchange event");
  is(event.oldVersion, dbVersion, "Correct old version");
  is(event.newVersion, null, "Correct new version");

  deletingRequest.onblocked = grabEventAndContinueHandler;

  event = yield undefined;

  is(event.type, "blocked", "Expect an blocked event");
  is(event.target, deletingRequest, "Event has right target");
  ok(event instanceof IDBVersionChangeEvent, "Expect a versionchange event");
  is(event.oldVersion, dbVersion, "Correct old version");
  is(event.newVersion, null, "Correct new version");

  deletingRequest.onsuccess = grabEventAndContinueHandler;
  db.close();

  event = yield undefined;

  is(event.type, "success", "expect a success event");
  is(event.target, deletingRequest, "event has right target");
  is(event.target.result, undefined, "event should have no result");

  openRequest = indexedDB.open(name, 1);
  openRequest.onerror = errorHandler;
  openRequest.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;
  db = event.target.result;
  is(db.version, 1, "DB has proper version");
  is(db.objectStoreNames.length, 0, "DB should have no object stores");

  db.close();

  finishTest();
  yield undefined;
}
