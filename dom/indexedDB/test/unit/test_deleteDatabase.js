﻿/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps() {
  const name = this.window ? window.location.pathname : "Splendid Test";

  ok(indexedDB.deleteDatabase, "deleteDatabase function should exist!");

  let request = indexedDB.open(name, 10);
  request.onerror = errorHandler;
  request.onsuccess = unexpectedSuccessHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;

  ok(request instanceof IDBOpenDBRequest, "Expect an IDBOpenDBRequest");

  let event = yield undefined;

  is(event.type, "upgradeneeded", "Expect an upgradeneeded event");
  ok(event instanceof IDBVersionChangeEvent, "Expect a versionchange event");

  let db = event.target.result;
  db.createObjectStore("stuff");

  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  is(event.type, "success", "Expect a success event");
  is(event.target, request, "Event has right target");
  ok(event.target.result instanceof IDBDatabase, "Result should be a database");
  is(db.objectStoreNames.length, 1, "Expect an objectStore here");

  request = indexedDB.open(name, 10);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.type, "success", "Expect a success event");
  is(event.target, request, "Event has right target");
  ok(event.target.result instanceof IDBDatabase, "Result should be a database");
  let db2 = event.target.result;
  is(db2.objectStoreNames.length, 1, "Expect an objectStore here");

  var onversionchangecalled = false;

  function closeDBs(event) {
    onversionchangecalled = true;
    ok(event instanceof IDBVersionChangeEvent, "expect a versionchange event");
    is(event.oldVersion, 10, "oldVersion should be 10");
    ok(event.newVersion === null, "newVersion should be null");
    ok(!(event.newVersion === undefined), "newVersion should be null");
    ok(!(event.newVersion === 0), "newVersion should be null");
    db.close();
    db2.close();
    db.onversionchange = unexpectedSuccessHandler;
    db2.onversionchange = unexpectedSuccessHandler;
  }

  // The IDB spec doesn't guarantee the order that onversionchange will fire
  // on the dbs.
  db.onversionchange = closeDBs;
  db2.onversionchange = closeDBs;

  request = indexedDB.deleteDatabase(name);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;

  ok(request instanceof IDBOpenDBRequest, "Expect an IDBOpenDBRequest");

  event = yield undefined;
  ok(onversionchangecalled, "Expected versionchange events");
  is(event.type, "success", "expect a success event");
  is(event.target, request, "event has right target");
  ok(event.target.result === undefined, "event should have no result");

  request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.target.result.version, 1, "DB has proper version");
  is(
    event.target.result.objectStoreNames.length,
    0,
    "DB should have no object stores"
  );

  request = indexedDB.deleteDatabase("thisDatabaseHadBetterNotExist");
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;
  ok(true, "deleteDatabase on a non-existent database succeeded");

  request = indexedDB.open("thisDatabaseHadBetterNotExist");
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;
  ok(true, "after deleting a non-existent database, open should work");

  finishTest();
}
