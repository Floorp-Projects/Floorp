/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const objectStoreData = [
    { name: "", options: { keyPath: "id", autoIncrement: true } },
    { name: null, options: { keyPath: "ss" } },
    { name: undefined, options: { } },
    { name: "4", options: { autoIncrement: true } },
  ];

  const indexData = [
    { name: "", keyPath: "name", options: { unique: true } },
    { name: null, keyPath: "height", options: { } }
  ];

  const data = [
    { ss: "237-23-7732", name: "Ann", height: 60 },
    { ss: "237-23-7733", name: "Bob", height: 65 }
  ];

  let request = mozIndexedDB.open(this.window ? window.location.pathname : "Splendid Test", 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield;

  let db = event.target.result;
  db.onerror = errorHandler;

  event.target.onsuccess = continueToNextStep;

  for (let objectStoreIndex in objectStoreData) {
    const objectStoreInfo = objectStoreData[objectStoreIndex];
    let objectStore = db.createObjectStore(objectStoreInfo.name,
                                           objectStoreInfo.options);
    for (let indexIndex in indexData) {
      const indexInfo = indexData[indexIndex];
      let index = objectStore.createIndex(indexInfo.name,
                                          indexInfo.keyPath,
                                          indexInfo.options);
    }
  }
  yield;

  ok(true, "Initial setup");

  for (let objectStoreIndex in objectStoreData) {
    const info = objectStoreData[objectStoreIndex];

    for (let indexIndex in indexData) {
      const objectStoreName = objectStoreData[objectStoreIndex].name;
      const indexName = indexData[indexIndex].name;

      let objectStore =
        db.transaction(objectStoreName, IDBTransaction.READ_WRITE)
          .objectStore(objectStoreName);
      ok(true, "Got objectStore " + objectStoreName);

      for (let dataIndex in data) {
        const obj = data[dataIndex];
        let key;
        if (!info.options.keyPath && !info.options.autoIncrement) {
          key = obj.ss;
        }
        objectStore.add(obj, key);
      }

      let index = objectStore.index(indexName);
      ok(true, "Got index " + indexName);

      let keyIndex = 0;

      index.openCursor().onsuccess = function(event) {
        let cursor = event.target.result;
        if (!cursor) {
          continueToNextStep();
          return;
        }

        is(cursor.key, data[keyIndex][indexData[indexIndex].keyPath],
           "Good key");
        is(cursor.value.ss, data[keyIndex].ss, "Correct ss");
        is(cursor.value.name, data[keyIndex].name, "Correct name");
        is(cursor.value.height, data[keyIndex].height, "Correct height");

        if (!keyIndex) {
          let obj = cursor.value;
          obj.updated = true;

          cursor.update(obj).onsuccess = function(event) {
            ok(true, "Object updated");
            cursor.continue();
            keyIndex++
          }
          return;
        }

        cursor.delete().onsuccess = function(event) {
          ok(true, "Object deleted");
          cursor.continue();
          keyIndex++
        }
      };
      yield;

      is(keyIndex, 2, "Saw all the items");

      keyIndex = 0;

      db.transaction(objectStoreName).objectStore(objectStoreName)
                                     .openCursor()
                                     .onsuccess = function(event) {
        let cursor = event.target.result;
        if (!cursor) {
          continueToNextStep();
          return;
        }

        is(cursor.value.ss, data[keyIndex].ss, "Correct ss");
        is(cursor.value.name, data[keyIndex].name, "Correct name");
        is(cursor.value.height, data[keyIndex].height, "Correct height");
        is(cursor.value.updated, true, "Correct updated flag");

        cursor.continue();
        keyIndex++;
      };
      yield;

      is(keyIndex, 1, "Saw all the items");

      db.transaction(objectStoreName, IDBTransaction.READ_WRITE)
        .objectStore(objectStoreName).clear()
        .onsuccess = continueToNextStep;
      yield;
    }
  }

  finishTest();
  yield;
}
