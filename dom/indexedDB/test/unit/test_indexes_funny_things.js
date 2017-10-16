/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  // Blob constructor is not implemented outside of windows yet (Bug 827723).
  if (!this.window) {
    finishTest();
    return;
  }

  const name = this.window ? window.location.pathname : "Splendid Test";

  const objectStoreName = "Things";

  const blob1 = new Blob(["foo", "bar"], { type: "text/plain" });
  const blob2 = new Blob(["foobazybar"], { type: "text/plain" });
  const blob3 = new Blob(["2"], { type: "bogus/" });
  const str = "The Book of Mozilla";
  str.type = blob1;
  const arr = [1, 2, 3, 4, 5];

  const objectStoreData = [
    { key: "1", value: blob1},
    { key: "2", value: blob2},
    { key: "3", value: blob3},
    { key: "4", value: str},
    { key: "5", value: arr},
  ];

  const indexData = [
    { name: "type", keyPath: "type", options: { } },
    { name: "length", keyPath: "length", options: { unique: true } }
  ];

  const objectStoreDataTypeSort = [
    { key: "3", value: blob3},
    { key: "1", value: blob1},
    { key: "2", value: blob2},
  ];

  const objectStoreDataLengthSort = [
    { key: "5", value: arr},
    { key: "4", value: str},
  ];

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;
  let db = event.target.result;

  let objectStore = db.createObjectStore(objectStoreName, { keyPath: null });

  // First, add all our data to the object store.
  let addedData = 0;
  for (let i in objectStoreData) {
    request = objectStore.add(objectStoreData[i].value,
                              objectStoreData[i].key);
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      if (++addedData == objectStoreData.length) {
        testGenerator.next(event);
      }
    };
  }
  event = yield undefined;
  // Now create the indexes.
  for (let i in indexData) {
    objectStore.createIndex(indexData[i].name, indexData[i].keyPath,
                            indexData[i].options);
  }
  is(objectStore.indexNames.length, indexData.length, "Good index count");
  yield undefined;
  objectStore = db.transaction(objectStoreName)
                  .objectStore(objectStoreName);

  // Check global properties to make sure they are correct.
  is(objectStore.indexNames.length, indexData.length, "Good index count");
  for (let i in indexData) {
    let found = false;
    for (let j = 0; j < objectStore.indexNames.length; j++) {
      if (objectStore.indexNames.item(j) == indexData[i].name) {
        found = true;
        break;
      }
    }
    is(found, true, "objectStore has our index");
    let index = objectStore.index(indexData[i].name);
    is(index.name, indexData[i].name, "Correct name");
    is(index.objectStore.name, objectStore.name, "Correct store name");
    is(index.keyPath, indexData[i].keyPath, "Correct keyPath");
    is(index.unique, !!indexData[i].options.unique, "Correct unique value");
  }

  ok(true, "Test group 1");

  let keyIndex = 0;

  request = objectStore.index("type").openKeyCursor();
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataTypeSort[keyIndex].value.type,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataTypeSort[keyIndex].key,
         "Correct primary key");
      ok(!("value" in cursor), "No value");

      cursor.continue();

      is(cursor.key, objectStoreDataTypeSort[keyIndex].value.type,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataTypeSort[keyIndex].key,
         "Correct value");
      ok(!("value" in cursor), "No value");

      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  };
  yield undefined;

  is(keyIndex, objectStoreDataTypeSort.length, "Saw all the expected keys");

  ok(true, "Test group 2");

  keyIndex = 0;

  request = objectStore.index("length").openKeyCursor(null, "next");
  request.onerror = errorHandler;
  request.onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataLengthSort[keyIndex].value.length,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataLengthSort[keyIndex].key,
         "Correct value");

      cursor.continue();

      is(cursor.key, objectStoreDataLengthSort[keyIndex].value.length,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataLengthSort[keyIndex].key,
         "Correct value");

      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  };
  yield undefined;

  is(keyIndex, objectStoreDataLengthSort.length, "Saw all the expected keys");

  finishTest();
}
