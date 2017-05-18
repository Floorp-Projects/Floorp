/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const objectStores = [ "foo", "bar" ];

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  is(db.objectStoreNames.length, 0, "Correct objectStoreNames list");

  event.target.onsuccess = grabEventAndContinueHandler;
  for (let i in objectStores) {
    db.createObjectStore(objectStores[i], { autoIncrement: true });
  }
  event = yield undefined;

  is(db.objectStoreNames.length, objectStores.length,
     "Correct objectStoreNames list");

  for (let i = 0; i < 50; i++) {
    let stepNumber = 0;

    request = db.transaction(["foo"], "readwrite")
                .objectStore("foo")
                .add({});
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      is(stepNumber, 1, "This callback came first");
      stepNumber++;
      event.target.transaction.oncomplete = grabEventAndContinueHandler;
    }

    request = db.transaction(["foo"], "readwrite")
                .objectStore("foo")
                .add({});
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      is(stepNumber, 2, "This callback came second");
      stepNumber++;
      event.target.transaction.oncomplete = grabEventAndContinueHandler;
    }

    request = db.transaction(["foo", "bar"], "readwrite")
                .objectStore("bar")
                .add({});
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      is(stepNumber, 3, "This callback came third");
      stepNumber++;
      event.target.transaction.oncomplete = grabEventAndContinueHandler;
    }

    request = db.transaction(["foo", "bar"], "readwrite")
                .objectStore("bar")
                .add({});
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      is(stepNumber, 4, "This callback came fourth");
      stepNumber++;
      event.target.transaction.oncomplete = grabEventAndContinueHandler;
    }

    request = db.transaction(["bar"], "readwrite")
                .objectStore("bar")
                .add({});
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      is(stepNumber, 5, "This callback came fifth");
      stepNumber++;
      event.target.transaction.oncomplete = grabEventAndContinueHandler;
    }

    stepNumber++;
    yield undefined; yield undefined; yield undefined; yield undefined; yield undefined;

    is(stepNumber, 6, "All callbacks received");
  }

  finishTest();
}

