/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps() {
  const name = this.window ? window.location.pathname : "Splendid Test";
  const objectStoreName = "People";

  const objectStoreData = [
    { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
    { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
    { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } },
    { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
    { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
    { key: "237-23-7737", value: { name: "Pat", height: 65 } },
  ];

  const indexData = [
    { name: "name", keyPath: "name", options: { unique: true } },
    { name: "height", keyPath: "height", options: { unique: false } },
    { name: "weight", keyPath: "weight", options: { unique: false } },
  ];

  const objectStoreDataHeightSort = [
    { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
    { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
    { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
    { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
    { key: "237-23-7737", value: { name: "Pat", height: 65 } },
    { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } },
  ];

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;

  let objectStore = db.createObjectStore(objectStoreName);

  // First, add all our data to the object store.
  let addedData = 0;
  for (let i in objectStoreData) {
    request = objectStore.add(objectStoreData[i].value, objectStoreData[i].key);
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      if (++addedData == objectStoreData.length) {
        testGenerator.next(event);
      }
    };
  }
  yield undefined;
  ok(true, "1");

  // Now create the indexes.
  for (let i in indexData) {
    objectStore.createIndex(
      indexData[i].name,
      indexData[i].keyPath,
      indexData[i].options
    );
  }

  is(objectStore.indexNames.length, indexData.length, "Good index count");
  yield undefined;

  ok(true, "2");
  objectStore = db.transaction(objectStoreName).objectStore(objectStoreName);

  request = objectStore.index("height").mozGetAllKeys(65);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;
  ok(true, "3");

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, 2, "Correct length");

  for (let i in event.target.result) {
    is(
      event.target.result[i],
      objectStoreDataHeightSort[parseInt(i) + 3].key,
      "Correct key"
    );
  }

  request = objectStore.index("height").mozGetAllKeys(65, 0);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;
  ok(true, "3");

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, 2, "Correct length");

  for (let i in event.target.result) {
    is(
      event.target.result[i],
      objectStoreDataHeightSort[parseInt(i) + 3].key,
      "Correct key"
    );
  }

  request = objectStore.index("height").mozGetAllKeys(65, null);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;
  ok(true, "3");

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, 2, "Correct length");

  for (let i in event.target.result) {
    is(
      event.target.result[i],
      objectStoreDataHeightSort[parseInt(i) + 3].key,
      "Correct key"
    );
  }

  request = objectStore.index("height").mozGetAllKeys(65, undefined);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;
  ok(true, "3");

  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, 2, "Correct length");

  for (let i in event.target.result) {
    is(
      event.target.result[i],
      objectStoreDataHeightSort[parseInt(i) + 3].key,
      "Correct key"
    );
  }

  request = objectStore.index("height").mozGetAllKeys();
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;
  ok(true, "4");

  is(event.target.result instanceof Array, true, "Got an array object");
  is(
    event.target.result.length,
    objectStoreDataHeightSort.length,
    "Correct length"
  );

  for (let i in event.target.result) {
    is(event.target.result[i], objectStoreDataHeightSort[i].key, "Correct key");
  }

  request = objectStore.index("height").mozGetAllKeys(null, 4);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  ok(true, "5");
  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, 4, "Correct length");

  for (let i in event.target.result) {
    is(event.target.result[i], objectStoreDataHeightSort[i].key, "Correct key");
  }

  request = objectStore.index("height").mozGetAllKeys(65, 1);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  ok(true, "6");
  is(event.target.result instanceof Array, true, "Got an array object");
  is(event.target.result.length, 1, "Correct length");

  for (let i in event.target.result) {
    is(
      event.target.result[i],
      objectStoreDataHeightSort[parseInt(i) + 3].key,
      "Correct key"
    );
  }

  finishTest();
}
