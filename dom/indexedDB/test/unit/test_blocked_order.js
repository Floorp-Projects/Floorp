/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let testGenerator = testSteps();

function testSteps()
{
  const databaseName =
    ("window" in this) ? window.location.pathname : "Test";
  const databaseCount = 10;

  // Test 1: Make sure basic versionchange events work and that they don't
  //         trigger blocked events.
  info("Opening " + databaseCount + " databases with version 1");

  let databases = [];

  for (let i = 0; i < databaseCount; i++) {
    let thisIndex = i;

    info("Opening database " + thisIndex);

    let request = indexedDB.open(databaseName, 1);
    request.onerror = errorHandler;
    request.onblocked = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;

    let event = yield undefined;

    is(event.type, "success", "Got success event");

    let db = request.result;
    is(db.version, 1, "Got version 1");

    db.onversionchange = function(event) {
      info("Closing database " + thisIndex);
      db.close();

      databases.splice(databases.indexOf(db), 1);
    };

    databases.push(db);
  }

  is(databases.length, databaseCount, "Created all databases with version 1");

  info("Opening database with version 2");

  let request = indexedDB.open(databaseName, 2);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;

  request.onblocked = function(event) {
    ok(false, "Should not receive a blocked event");
  };

  event = yield undefined;

  is(event.type, "success", "Got success event");
  is(databases.length, 0, "All databases with version 1 were closed");

  let db = request.result;
  is(db.version, 2, "Got version 2");

  info("Deleting database with version 2");
  db.close();

  request = indexedDB.deleteDatabase(databaseName);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  is(event.type, "success", "Got success event");

  // Test 2: Make sure blocked events aren't delivered until all versionchange
  //         events have been delivered.
  info("Opening " + databaseCount + " databases with version 1");

  for (let i = 0; i < databaseCount; i++) {
    let thisIndex = i;

    info("Opening database " + thisIndex);

    let request = indexedDB.open(databaseName, 1);
    request.onerror = errorHandler;
    request.onblocked = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;

    let event = yield undefined;

    is(event.type, "success", "Got success event");

    let db = request.result;
    is(db.version, 1, "Got version 1");

    db.onversionchange = function(event) {
      if (thisIndex == (databaseCount - 1)) {
        info("Closing all databases with version 1");

        for (let j = 0; j < databases.length; j++) {
          databases[j].close();
        }

        databases = [];
        info("Done closing all databases with version 1");
      } else {
        info("Not closing database " + thisIndex);
      }
    };

    databases.push(db);
  }

  is(databases.length, databaseCount, "Created all databases with version 1");

  info("Opening database with version 2");

  request = indexedDB.open(databaseName, 2);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;

  request.onblocked = function(event) {
    ok(false, "Should not receive a blocked event");
  };

  event = yield undefined;

  is(event.type, "success", "Got success event");
  is(databases.length, 0, "All databases with version 1 were closed");

  db = request.result;
  is(db.version, 2, "Got version 2");

  info("Deleting database with version 2");
  db.close();

  request = indexedDB.deleteDatabase(databaseName);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  is(event.type, "success", "Got success event");

  finishTest();
  yield undefined;
}
