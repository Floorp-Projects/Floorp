/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const description = "My Test Database";

  let request = indexedDB.open(name, 1, description);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield;

  let db = event.target.result;

  // Check default state.
  is(db.version, 1, "Correct default version for a new database.");

  const versions = [
    7,
    42,
  ];

  db.close();

  for (let i = 0; i < versions.length; i++) {
    let version = versions[i];

    let request = indexedDB.open(name, version, description);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    let event = yield;

    let db = event.target.result;

    is(db.version, version, "Database version number updated correctly");
    is(event.target.transaction.mode, "versionchange", "Correct mode");

    executeSoon(function() { testGenerator.next(); });
    yield;
    db.close();
  }

  finishTest();
  yield;
}

