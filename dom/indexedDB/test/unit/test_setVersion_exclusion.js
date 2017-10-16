/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;

  let request2 = indexedDB.open(name, 2);
  request2.onerror = errorHandler;
  request2.onupgradeneeded = unexpectedSuccessHandler;
  request2.onsuccess = unexpectedSuccessHandler;

  let event = yield undefined;
  is(event.type, "upgradeneeded", "Expect an upgradeneeded event");
  is(event.target, request, "Event should be fired on the request");
  ok(event.target.result instanceof IDBDatabase, "Expect a database here");

  let db = event.target.result;
  is(db.version, 1, "Database has correct version");

  db.onupgradeneeded = function() {
    ok(false, "our ongoing VERSION_CHANGE transaction should exclude any others!");
  };

  db.createObjectStore("foo");

  try {
    db.transaction("foo");
    ok(false, "Transactions should be disallowed now!");
  } catch (e) {
    ok(e instanceof DOMException, "Expect a DOMException");
    is(e.name, "InvalidStateError", "Expect an InvalidStateError");
    is(e.code, DOMException.INVALID_STATE_ERR, "Expect an INVALID_STATE_ERR");
  }

  request.onupgradeneeded = unexpectedSuccessHandler;
  request.transaction.oncomplete = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.type, "complete", "Got complete event");

  try {
    db.transaction("foo");
    ok(true, "Transactions should be allowed now!");
  } catch (e) {
    ok(false, "Transactions should be allowed now!");
  }

  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.type, "success", "Expect a success event");
  is(event.target.result, db, "Same database");

  db.onversionchange = function() {
    ok(true, "next setVersion was unblocked appropriately");
    db.close();
  };

  try {
    db.transaction("foo");
    ok(true, "Transactions should be allowed now!");
  } catch (e) {
    ok(false, "Transactions should be allowed now!");
  }

  request.onsuccess = unexpectedSuccessHandler;
  request2.onupgradeneeded = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.type, "upgradeneeded", "Expect an upgradeneeded event");

  db = event.target.result;
  is(db.version, 2, "Database has correct version");

  request2.onupgradeneeded = unexpectedSuccessHandler;
  request2.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.type, "success", "Expect a success event");
  is(event.target.result, db, "Same database");
  is(db.version, 2, "Database has correct version");

  finishTest();
}
