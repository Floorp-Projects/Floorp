/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";

  const objectStoreName = "People";

  const objectStoreData = [
    { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
    { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
    { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } },
    { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
    { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
    { key: "237-23-7737", value: { name: "Pat", height: 65 } },
    { key: "237-23-7738", value: { name: "Mel", height: 66, weight: {} } }
  ];

  const badObjectStoreData = [
    { key: "237-23-7739", value: { name: "Rob", height: 65 } },
    { key: "237-23-7740", value: { name: "Jen", height: 66, weight: {} } }
  ];

  const indexData = [
    { name: "weight", keyPath: "weight", options: { unique: false } }
  ];

  const objectStoreDataWeightSort = [
    { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
    { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
    { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
    { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
    { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } }
  ];

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;

  let objectStore = db.createObjectStore(objectStoreName, { } );

  let addedData = 0;
  for (let i in objectStoreData) {
    request = objectStore.add(objectStoreData[i].value,
                              objectStoreData[i].key);
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      if (++addedData == objectStoreData.length) {
        testGenerator.next(event);
      }
    }
  }
  event = yield undefined;

  for (let i in indexData) {
    objectStore.createIndex(indexData[i].name, indexData[i].keyPath,
                            indexData[i].options);
  }

  addedData = 0;
  for (let i in badObjectStoreData) {
    request = objectStore.add(badObjectStoreData[i].value,
                              badObjectStoreData[i].key);
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      if (++addedData == badObjectStoreData.length) {
        executeSoon(function() { testGenerator.next() });
      }
    }
  }
  yield undefined;
  yield undefined;

  objectStore = db.transaction(objectStoreName)
                  .objectStore(objectStoreName);

  let keyIndex = 0;

  request = objectStore.index("weight").openKeyCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataWeightSort[keyIndex].value.weight,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataWeightSort[keyIndex].key,
         "Correct value");
      keyIndex++;

      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, objectStoreDataWeightSort.length, "Saw all weights");

  keyIndex = 0;

  request = objectStore.openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      keyIndex++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, objectStoreData.length + badObjectStoreData.length,
     "Saw all people");

  finishTest();
}
