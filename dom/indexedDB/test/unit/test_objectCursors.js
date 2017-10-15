/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";

  const objectStores = [
    { name: "a", autoIncrement: false },
    { name: "b", autoIncrement: true }
  ];

  const indexes = [
    { name: "a", options: { } },
    { name: "b", options: { unique: true } }
  ];

  var j = 0;
  for (let i in objectStores) {
    let request = indexedDB.open(name, ++j);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    let event = yield undefined;

    let db = event.target.result;
    db.onversionchange = function(event) {
      event.target.close();
    };

    let objectStore =
      db.createObjectStore(objectStores[i].name,
                           { keyPath: "id",
                             autoIncrement: objectStores[i].autoIncrement });

    for (let j in indexes) {
      objectStore.createIndex(indexes[j].name, "name", indexes[j].options);
    }

    let data = { name: "Ben" };
    if (!objectStores[i].autoIncrement) {
      data.id = 1;
    }

    request = objectStore.add(data);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    ok(event.target.result == 1 || event.target.result == 2, "Good id");
  }

  executeSoon(function() { testGenerator.next(); });
  yield undefined;

  let request = indexedDB.open(name, j);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;

  for (let i in objectStores) {
    for (let j in indexes) {
      let objectStore = db.transaction(objectStores[i].name)
                          .objectStore(objectStores[i].name);
      let index = objectStore.index(indexes[j].name);

      request = index.openCursor();
      request.onerror = errorHandler;
      request.onsuccess = function(event) {
        is(event.target.result.value.name, "Ben", "Good object");
        executeSoon(function() { testGenerator.next(); });
      };
      yield undefined;
    }
  }

  finishTest();
}

