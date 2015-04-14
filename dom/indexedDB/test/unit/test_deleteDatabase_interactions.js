/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";

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

  db.close();

  request = indexedDB.deleteDatabase(name);

  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;

  ok(request instanceof IDBOpenDBRequest, "Expect an IDBOpenDBRequest");

  let openRequest = indexedDB.open(name, 1);
  openRequest.onerror = errorHandler;
  openRequest.onsuccess = unexpectedSuccessHandler;

  event = yield undefined;
  is(event.type, "success", "expect a success event");
  is(event.target, request, "event has right target");
  is(event.target.result, undefined, "event should have no result");

  openRequest.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.target.result.version, 1, "DB has proper version");
  is(event.target.result.objectStoreNames.length, 0, "DB should have no object stores");

  finishTest();
  yield undefined;
}
