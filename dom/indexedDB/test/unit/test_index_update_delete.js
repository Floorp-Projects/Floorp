/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  let name = this.window ? window.location.pathname : "Splendid Test";
  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;

  let event = yield;

  let db = event.target.result;
  db.onerror = errorHandler;

  for each (let autoIncrement in [false, true]) {
    let objectStore =
      db.createObjectStore(autoIncrement, { keyPath: "id",
                                            autoIncrement: autoIncrement });

    for (let i = 0; i < 10; i++) {
      objectStore.add({ id: i, index: i });
    }

    for each (let unique in [false, true]) {
      objectStore.createIndex(unique, "index", { unique: unique });
    }

    for (let i = 10; i < 20; i++) {
      objectStore.add({ id: i, index: i });
    }
  }

  event = yield;
  is(event.type, "success", "expect a success event");

  for each (let autoIncrement in [false, true]) {
    let objectStore = db.transaction(autoIncrement)
                        .objectStore(autoIncrement);

    objectStore.count().onsuccess = grabEventAndContinueHandler;
    let event = yield;

    is(event.target.result, 20, "Correct number of entries in objectStore");

    let objectStoreCount = event.target.result;
    let indexCount = event.target.result;

    for each (let unique in [false, true]) {
      let index = db.transaction(autoIncrement, "readwrite")
                    .objectStore(autoIncrement)
                    .index(unique);

      index.count().onsuccess = grabEventAndContinueHandler;
      let event = yield;

      is(event.target.result, indexCount,
         "Correct number of entries in index");

      let modifiedEntry = unique ? 5 : 10;
      let keyRange = IDBKeyRange.only(modifiedEntry);

      let sawEntry = false;
      index.openCursor(keyRange).onsuccess = function(event) {
        let cursor = event.target.result;
        if (cursor) {
          sawEntry = true;
          is(cursor.key, modifiedEntry, "Correct key");

          cursor.value.index = unique ? 30 : 35;
          cursor.update(cursor.value).onsuccess = function(event) {
            cursor.continue();
          }
        }
        else {
          continueToNextStep();
        }
      }
      yield;

      is(sawEntry, true, "Saw entry for key value " + modifiedEntry);

      // Recount index. Shouldn't change.
      index = db.transaction(autoIncrement, "readwrite")
                .objectStore(autoIncrement)
                .index(unique);

      index.count().onsuccess = grabEventAndContinueHandler;
      event = yield;

      is(event.target.result, indexCount,
         "Correct number of entries in index");

      modifiedEntry = unique ? 30 : 35;
      keyRange = IDBKeyRange.only(modifiedEntry);

      sawEntry = false;
      index.openCursor(keyRange).onsuccess = function(event) {
        let cursor = event.target.result;
        if (cursor) {
          sawEntry = true;
          is(cursor.key, modifiedEntry, "Correct key");

          delete cursor.value.index;
          cursor.update(cursor.value).onsuccess = function(event) {
            indexCount--;
            cursor.continue();
          }
        }
        else {
          continueToNextStep();
        }
      }
      yield;

      is(sawEntry, true, "Saw entry for key value " + modifiedEntry);

      // Recount objectStore. Should be unchanged.
      objectStore = db.transaction(autoIncrement, "readwrite")
                      .objectStore(autoIncrement);

      objectStore.count().onsuccess = grabEventAndContinueHandler;
      event = yield;

      is(event.target.result, objectStoreCount,
         "Correct number of entries in objectStore");

      // Recount index. Should be one item less.
      index = objectStore.index(unique);

      index.count().onsuccess = grabEventAndContinueHandler;
      event = yield;

      is(event.target.result, indexCount,
         "Correct number of entries in index");

      modifiedEntry = objectStoreCount - 1;

      objectStore.delete(modifiedEntry).onsuccess =
        grabEventAndContinueHandler;
      event = yield;

      objectStoreCount--;
      indexCount--;

      objectStore.count().onsuccess = grabEventAndContinueHandler;
      event = yield;

      is(event.target.result, objectStoreCount,
         "Correct number of entries in objectStore");

      index.count().onsuccess = grabEventAndContinueHandler;
      event = yield;

      is(event.target.result, indexCount,
         "Correct number of entries in index");
    }
  }

  finishTest();
  yield;
}
