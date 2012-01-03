/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const READ_WRITE = Components.interfaces.nsIIDBTransaction.READ_WRITE;

  const name = this.window ? window.location.pathname : "Splendid Test";
  const description = "My Test Database";
  const objectStores = [ "foo", "bar" ];

  let request = mozIndexedDB.open(name, 1, description);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield;

  let db = event.target.result;
  is(db.objectStoreNames.length, 0, "Correct objectStoreNames list");

  event.target.onsuccess = grabEventAndContinueHandler;
  for (let i in objectStores) {
    db.createObjectStore(objectStores[i], { autoIncrement: true });
  }
  let event = yield;

  is(db.objectStoreNames.length, objectStores.length,
     "Correct objectStoreNames list");

  for (let i = 0; i < 50; i++) {
    let stepNumber = 0;

    request = db.transaction(["foo"], READ_WRITE)
                .objectStore("foo")
                .add({});
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      is(stepNumber, 1, "This callback came first");
      stepNumber++;
    }

    request = db.transaction(["foo"], READ_WRITE)
                .objectStore("foo")
                .add({});
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      is(stepNumber, 2, "This callback came second");
      stepNumber++;
    }

    request = db.transaction(["foo", "bar"], READ_WRITE)
                .objectStore("bar")
                .add({});
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      is(stepNumber, 3, "This callback came third");
      stepNumber++;
    }

    request = db.transaction(["foo", "bar"], READ_WRITE)
                .objectStore("bar")
                .add({});
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      is(stepNumber, 4, "This callback came fourth");
      stepNumber++;
    }

    request = db.transaction(["bar"], READ_WRITE)
                .objectStore("bar")
                .add({});
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      is(stepNumber, 5, "This callback came fifth");
      stepNumber++;
      event.target.transaction.oncomplete = grabEventAndContinueHandler;
    }

    stepNumber++;
    yield;

    is(stepNumber, 6, "All callbacks received");
  }

  finishTest();
  yield;
}

