/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const IDBObjectStore = Components.interfaces.nsIIDBObjectStore;
  const IDBDatabaseException = Components.interfaces.nsIIDBDatabaseException;
  const name = this.window ? window.location.pathname : "Splendid Test";
  const description = "My Test Database";

  var data = [
    { name: "inline key; key generator",
      autoIncrement: true,
      storedObject: {name: "Lincoln"},
      keyName: "id",
      keyValue: undefined,
    },
    { name: "inline key; no key generator",
      autoIncrement: false,
      storedObject: {id: 1, name: "Lincoln"},
      keyName: "id",
      keyValue: undefined,
    },
    { name: "out of line key; key generator",
      autoIncrement: true,
      storedObject: {name: "Lincoln"},
      keyName: undefined,
      keyValue: undefined,
    },
    { name: "out of line key; no key generator",
      autoIncrement: false,
      storedObject: {name: "Lincoln"},
      keyName: null,
      keyValue: 1,
    }
  ];

  for (let i = 0; i < data.length; i++) {
    let test = data[i];

    let request = mozIndexedDB.open(name, i+1, description);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    let event = yield;

    let db = event.target.result;

    let objectStore = db.createObjectStore(test.name,
                                           { keyPath: test.keyName,
                                             autoIncrement: test.autoIncrement });

    request = objectStore.add(test.storedObject, test.keyValue);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield;

    let id = event.target.result;
    request = objectStore.get(id);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield;

    // Sanity check!
    is(test.storedObject.name, event.target.result.name,
                  "The correct object was stored.");

    request = objectStore.delete(id);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield;

    // Make sure it was removed.
    request = objectStore.get(id);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield;

    ok(event.target.result === undefined, "Object was deleted");
    db.close();
  }

  finishTest();
  yield;
}

