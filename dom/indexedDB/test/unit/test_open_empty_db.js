/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const names = [
    //"",
    null,
    undefined,
    this.window ? window.location.pathname : "Splendid Test"
  ];

  const version = 1;

  for each (let name in names) {
    let request = mozIndexedDB.open(name, version);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    let event = yield;

    if (name === null) {
      name = "null";
    }
    else if (name === undefined) {
      name = "undefined";
    }

    let db = event.target.result;
    is(db.name, name, "Bad name");
    is(db.version, version, "Bad version");
    is(db.objectStoreNames.length, 0, "Bad objectStores list");

    is(db.name, request.result.name, "Bad name");
    is(db.version, request.result.version, "Bad version");
    is(db.objectStoreNames.length, request.result.objectStoreNames.length,
       "Bad objectStores list");
  }

  finishTest();
  yield;
}

