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
    { key: "237-23-7738", value: { name: "Mel", height: 66, weight: {} } },
    { key: "237-23-7739", value: { name: "Tom", height: 62, weight: 130 } }
  ];

  const indexData = {
    name: "weight",
    keyPath: "weight",
    options: { unique: false }
  };

  const weightSort = [1, 0, 3, 7, 4, 2];

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  is(event.type, "upgradeneeded", "Got correct event type");

  let db = event.target.result;
  db.onerror = errorHandler;

  let objectStore = db.createObjectStore(objectStoreName, { });
  objectStore.createIndex(indexData.name, indexData.keyPath,
                          indexData.options);

  for (let data of objectStoreData) {
    objectStore.add(data.value, data.key);
  }

  event = yield undefined;

  is(event.type, "success", "Got correct event type");

  objectStore = db.transaction(db.objectStoreNames)
                  .objectStore(objectStoreName);

  objectStore.count().onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, objectStoreData.length,
     "Correct number of object store entries for all keys");

  objectStore.count(null).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, objectStoreData.length,
     "Correct number of object store entries for null key");

  objectStore.count(objectStoreData[2].key).onsuccess =
    grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 1,
     "Correct number of object store entries for single existing key");

  objectStore.count("foo").onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 0,
     "Correct number of object store entries for single non-existing key");

  let keyRange = IDBKeyRange.only(objectStoreData[2].key);
  objectStore.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 1,
     "Correct number of object store entries for existing only keyRange");

  keyRange = IDBKeyRange.only("foo");
  objectStore.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 0,
     "Correct number of object store entries for non-existing only keyRange");

  keyRange = IDBKeyRange.lowerBound(objectStoreData[2].key);
  objectStore.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, objectStoreData.length - 2,
     "Correct number of object store entries for lowerBound keyRange");

  keyRange = IDBKeyRange.lowerBound(objectStoreData[2].key, true);
  objectStore.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, objectStoreData.length - 3,
     "Correct number of object store entries for lowerBound keyRange");

  keyRange = IDBKeyRange.lowerBound("foo");
  objectStore.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 0,
     "Correct number of object store entries for lowerBound keyRange");

  keyRange = IDBKeyRange.upperBound(objectStoreData[2].key, false);
  objectStore.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 3,
     "Correct number of object store entries for upperBound keyRange");

  keyRange = IDBKeyRange.upperBound(objectStoreData[2].key, true);
  objectStore.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 2,
     "Correct number of object store entries for upperBound keyRange");

  keyRange = IDBKeyRange.upperBound("foo", true);
  objectStore.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, objectStoreData.length,
     "Correct number of object store entries for upperBound keyRange");

  keyRange = IDBKeyRange.bound(objectStoreData[0].key,
                               objectStoreData[objectStoreData.length - 1].key);
  objectStore.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, objectStoreData.length,
     "Correct number of object store entries for bound keyRange");

  keyRange = IDBKeyRange.bound(objectStoreData[0].key,
                               objectStoreData[objectStoreData.length - 1].key,
                               true);
  objectStore.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, objectStoreData.length - 1,
     "Correct number of object store entries for bound keyRange");

  keyRange = IDBKeyRange.bound(objectStoreData[0].key,
                               objectStoreData[objectStoreData.length - 1].key,
                               true, true);
  objectStore.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, objectStoreData.length - 2,
     "Correct number of object store entries for bound keyRange");

  keyRange = IDBKeyRange.bound("foo", "foopy", true, true);
  objectStore.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 0,
     "Correct number of object store entries for bound keyRange");

  keyRange = IDBKeyRange.bound(objectStoreData[0].key, "foo", true, true);
  objectStore.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, objectStoreData.length - 1,
     "Correct number of object store entries for bound keyRange");

  let index = objectStore.index(indexData.name);

  index.count().onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, weightSort.length,
     "Correct number of index entries for no key");

  index.count(objectStoreData[7].value.weight).onsuccess =
    grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 2,
     "Correct number of index entries for duplicate key");

  index.count(objectStoreData[0].value.weight).onsuccess =
    grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 1,
     "Correct number of index entries for single key");

  keyRange = IDBKeyRange.only(objectStoreData[0].value.weight);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 1,
     "Correct number of index entries for only existing keyRange");

  keyRange = IDBKeyRange.only("foo");
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 0,
     "Correct number of index entries for only non-existing keyRange");

  keyRange = IDBKeyRange.only(objectStoreData[7].value.weight);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 2,
     "Correct number of index entries for only duplicate keyRange");

  keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[0]].value.weight);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, weightSort.length,
     "Correct number of index entries for lowerBound keyRange");

  keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[1]].value.weight);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, weightSort.length - 1,
     "Correct number of index entries for lowerBound keyRange");

  keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[0]].value.weight - 1);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, weightSort.length,
     "Correct number of index entries for lowerBound keyRange");

  keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[0]].value.weight,
                                    true);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, weightSort.length - 1,
     "Correct number of index entries for lowerBound keyRange");

  keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[weightSort.length - 1]].value.weight);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 1,
     "Correct number of index entries for lowerBound keyRange");

  keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[weightSort.length - 1]].value.weight,
                                    true);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 0,
     "Correct number of index entries for lowerBound keyRange");

  keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[weightSort.length - 1]].value.weight + 1,
                                    true);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 0,
     "Correct number of index entries for lowerBound keyRange");

  keyRange = IDBKeyRange.upperBound(objectStoreData[weightSort[0]].value.weight);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 1,
     "Correct number of index entries for upperBound keyRange");

  keyRange = IDBKeyRange.upperBound(objectStoreData[weightSort[0]].value.weight,
                                    true);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 0,
     "Correct number of index entries for upperBound keyRange");

  keyRange = IDBKeyRange.upperBound(objectStoreData[weightSort[weightSort.length - 1]].value.weight);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, weightSort.length,
     "Correct number of index entries for upperBound keyRange");

  keyRange = IDBKeyRange.upperBound(objectStoreData[weightSort[weightSort.length - 1]].value.weight,
                                    true);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, weightSort.length - 1,
     "Correct number of index entries for upperBound keyRange");

  keyRange = IDBKeyRange.upperBound(objectStoreData[weightSort[weightSort.length - 1]].value.weight,
                                    true);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, weightSort.length - 1,
     "Correct number of index entries for upperBound keyRange");

  keyRange = IDBKeyRange.upperBound("foo");
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, weightSort.length,
     "Correct number of index entries for upperBound keyRange");

  keyRange = IDBKeyRange.bound("foo", "foopy");
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 0,
     "Correct number of index entries for bound keyRange");

  keyRange = IDBKeyRange.bound(objectStoreData[weightSort[0]].value.weight,
                               objectStoreData[weightSort[weightSort.length - 1]].value.weight);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, weightSort.length,
     "Correct number of index entries for bound keyRange");

  keyRange = IDBKeyRange.bound(objectStoreData[weightSort[0]].value.weight,
                               objectStoreData[weightSort[weightSort.length - 1]].value.weight,
                               true);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, weightSort.length - 1,
     "Correct number of index entries for bound keyRange");

  keyRange = IDBKeyRange.bound(objectStoreData[weightSort[0]].value.weight,
                               objectStoreData[weightSort[weightSort.length - 1]].value.weight,
                               true, true);
  index.count(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, weightSort.length - 2,
     "Correct number of index entries for bound keyRange");

  finishTest();
}
