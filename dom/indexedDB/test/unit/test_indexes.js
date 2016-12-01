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
    { key: "237-23-7737", value: { name: "Pat", height: 65 } }
  ];

  const indexData = [
    { name: "name", keyPath: "name", options: { unique: true } },
    { name: "height", keyPath: "height", options: { } },
    { name: "weight", keyPath: "weight", options: { unique: false } }
  ];

  const objectStoreDataNameSort = [
    { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
    { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
    { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
    { key: "237-23-7737", value: { name: "Pat", height: 65 } },
    { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } },
    { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } }
  ];

  const objectStoreDataWeightSort = [
    { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
    { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
    { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
    { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
    { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } }
  ];

  const objectStoreDataHeightSort = [
    { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
    { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
    { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
    { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
    { key: "237-23-7737", value: { name: "Pat", height: 65 } },
    { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } }
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
    }
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
    is(index.unique, indexData[i].options.unique ? true : false,
       "Correct unique value");
  }

  request = objectStore.index("name").getKey("Bob");
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, "237-23-7732", "Correct key returned!");

  request = objectStore.index("name").get("Bob");
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result.name, "Bob", "Correct name returned!");
  is(event.target.result.height, 60, "Correct height returned!");
  is(event.target.result.weight, 120, "Correct weight returned!");

  ok(true, "Test group 1");

  let keyIndex = 0;

  request = objectStore.index("name").openKeyCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      ok(!("value" in cursor), "No value");

      cursor.continue();

      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");
      ok(!("value" in cursor), "No value");

      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, objectStoreData.length, "Saw all the expected keys");

  ok(true, "Test group 2");

  keyIndex = 0;

  request = objectStore.index("weight").openKeyCursor(null, "next");
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataWeightSort[keyIndex].value.weight,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataWeightSort[keyIndex].key,
         "Correct value");

      cursor.continue();

      is(cursor.key, objectStoreDataWeightSort[keyIndex].value.weight,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataWeightSort[keyIndex].key,
         "Correct value");

      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, objectStoreData.length - 1, "Saw all the expected keys");

  // Check that the name index enforces its unique constraint.
  objectStore = db.transaction(objectStoreName, "readwrite")
                  .objectStore(objectStoreName);
  request = objectStore.add({ name: "Bob", height: 62, weight: 170 },
                            "237-23-7738");
  request.addEventListener("error", new ExpectError("ConstraintError", true));
  request.onsuccess = unexpectedSuccessHandler;
  event = yield undefined;

  ok(true, "Test group 3");

  keyIndex = objectStoreDataNameSort.length - 1;

  request = objectStore.index("name").openKeyCursor(null, "prev");
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      cursor.continue();

      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      keyIndex--;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, -1, "Saw all the expected keys");

  ok(true, "Test group 4");

  keyIndex = 1;
  let keyRange = IDBKeyRange.bound("Bob", "Ron");

  request = objectStore.index("name").openKeyCursor(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      cursor.continue();
      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 5, "Saw all the expected keys");

  ok(true, "Test group 5");

  keyIndex = 2;
  keyRange = IDBKeyRange.bound("Bob", "Ron", true);

  request = objectStore.index("name").openKeyCursor(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      cursor.continue();
      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 5, "Saw all the expected keys");

  ok(true, "Test group 6");

  keyIndex = 1;
  keyRange = IDBKeyRange.bound("Bob", "Ron", false, true);

  request = objectStore.index("name").openKeyCursor(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      cursor.continue();
      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 4, "Saw all the expected keys");

  ok(true, "Test group 7");

  keyIndex = 2;
  keyRange = IDBKeyRange.bound("Bob", "Ron", true, true);

  request = objectStore.index("name").openKeyCursor(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      cursor.continue();
      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 4, "Saw all the expected keys");

  ok(true, "Test group 8");

  keyIndex = 1;
  keyRange = IDBKeyRange.lowerBound("Bob");

  request = objectStore.index("name").openKeyCursor(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      cursor.continue();
      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, objectStoreDataNameSort.length, "Saw all the expected keys");

  ok(true, "Test group 9");

  keyIndex = 2;
  keyRange = IDBKeyRange.lowerBound("Bob", true);

  request = objectStore.index("name").openKeyCursor(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      cursor.continue();
      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, objectStoreDataNameSort.length, "Saw all the expected keys");

  ok(true, "Test group 10");

  keyIndex = 0;
  keyRange = IDBKeyRange.upperBound("Joe");

  request = objectStore.index("name").openKeyCursor(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      cursor.continue();
      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 3, "Saw all the expected keys");

  ok(true, "Test group 11");

  keyIndex = 0;
  keyRange = IDBKeyRange.upperBound("Joe", true);

  request = objectStore.index("name").openKeyCursor(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      cursor.continue();
      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 2, "Saw all the expected keys");

  ok(true, "Test group 12");

  keyIndex = 3;
  keyRange = IDBKeyRange.only("Pat");

  request = objectStore.index("name").openKeyCursor(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      cursor.continue();
      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 4, "Saw all the expected keys");

  ok(true, "Test group 13");

  keyIndex = 0;

  request = objectStore.index("name").openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      cursor.continue();

      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, objectStoreDataNameSort.length, "Saw all the expected keys");

  ok(true, "Test group 14");

  keyIndex = objectStoreDataNameSort.length - 1;

  request = objectStore.index("name").openCursor(null, "prev");
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      cursor.continue();

      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      keyIndex--;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, -1, "Saw all the expected keys");

  ok(true, "Test group 15");

  keyIndex = 1;
  keyRange = IDBKeyRange.bound("Bob", "Ron");

  request = objectStore.index("name").openCursor(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      cursor.continue();

      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 5, "Saw all the expected keys");

  ok(true, "Test group 16");

  keyIndex = 2;
  keyRange = IDBKeyRange.bound("Bob", "Ron", true);

  request = objectStore.index("name").openCursor(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      cursor.continue();

      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 5, "Saw all the expected keys");

  ok(true, "Test group 17");

  keyIndex = 1;
  keyRange = IDBKeyRange.bound("Bob", "Ron", false, true);

  request = objectStore.index("name").openCursor(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      cursor.continue();

      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 4, "Saw all the expected keys");

  ok(true, "Test group 18");

  keyIndex = 2;
  keyRange = IDBKeyRange.bound("Bob", "Ron", true, true);

  request = objectStore.index("name").openCursor(keyRange);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      cursor.continue();

      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 4, "Saw all the expected keys");

  ok(true, "Test group 19");

  keyIndex = 4;
  keyRange = IDBKeyRange.bound("Bob", "Ron");

  request = objectStore.index("name").openCursor(keyRange, "prev");
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      cursor.continue();

      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      keyIndex--;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 0, "Saw all the expected keys");

  ok(true, "Test group 20");

  // Test "nextunique"
  keyIndex = 3;
  keyRange = IDBKeyRange.only(65);

  request = objectStore.index("height").openKeyCursor(keyRange, "next");
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
         "Correct value");

      cursor.continue();
      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 5, "Saw all the expected keys");

  ok(true, "Test group 21");

  keyIndex = 3;
  keyRange = IDBKeyRange.only(65);

  request = objectStore.index("height").openKeyCursor(keyRange,
                                                      "nextunique");
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
         "Correct value");

      cursor.continue();
      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 4, "Saw all the expected keys");

  ok(true, "Test group 21.5");

  keyIndex = 5;

  request = objectStore.index("height").openKeyCursor(null, "prev");
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
         "Correct value");

      cursor.continue();
      keyIndex--;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, -1, "Saw all the expected keys");

  ok(true, "Test group 22");

  keyIndex = 5;

  request = objectStore.index("height").openKeyCursor(null,
                                                      "prevunique");
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
         "Correct value");

      cursor.continue();
      if (keyIndex == 5) {
        keyIndex--;
      }
      keyIndex--;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, -1, "Saw all the expected keys");

  ok(true, "Test group 23");

  keyIndex = 3;
  keyRange = IDBKeyRange.only(65);

  request = objectStore.index("height").openCursor(keyRange, "next");
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataHeightSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataHeightSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataHeightSort[keyIndex].value.weight,
           "Correct weight");
      }

      cursor.continue();
      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 5, "Saw all the expected keys");

  ok(true, "Test group 24");

  keyIndex = 3;
  keyRange = IDBKeyRange.only(65);

  request = objectStore.index("height").openCursor(keyRange,
                                                   "nextunique");
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataHeightSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataHeightSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataHeightSort[keyIndex].value.weight,
           "Correct weight");
      }

      cursor.continue();
      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 4, "Saw all the expected keys");

  ok(true, "Test group 24.5");

  keyIndex = 5;

  request = objectStore.index("height").openCursor(null, "prev");
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataHeightSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataHeightSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataHeightSort[keyIndex].value.weight,
           "Correct weight");
      }

      cursor.continue();
      keyIndex--;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, -1, "Saw all the expected keys");

  ok(true, "Test group 25");

  keyIndex = 5;

  request = objectStore.index("height").openCursor(null,
                                                   "prevunique");
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataHeightSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataHeightSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataHeightSort[keyIndex].value.weight,
           "Correct weight");
      }

      cursor.continue();
      if (keyIndex == 5) {
        keyIndex--;
      }
      keyIndex--;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, -1, "Saw all the expected keys");

  ok(true, "Test group 26");

  keyIndex = 0;

  request = objectStore.index("name").openKeyCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      let nextKey = !keyIndex ? "Pat" : undefined;

      cursor.continue(nextKey);

      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      if (!keyIndex) {
        keyIndex = 3;
      }
      else {
        keyIndex++;
      }
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, objectStoreData.length, "Saw all the expected keys");

  ok(true, "Test group 27");

  keyIndex = 0;

  request = objectStore.index("name").openKeyCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      let nextKey = !keyIndex ? "Flo" : undefined;

      cursor.continue(nextKey);

      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct value");

      keyIndex += keyIndex ? 1 : 2;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, objectStoreData.length, "Saw all the expected keys");

  ok(true, "Test group 28");

  keyIndex = 0;

  request = objectStore.index("name").openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      let nextKey = !keyIndex ? "Pat" : undefined;

      cursor.continue(nextKey);

      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      if (!keyIndex) {
        keyIndex = 3;
      }
      else {
        keyIndex++;
      }
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, objectStoreDataNameSort.length, "Saw all the expected keys");

  ok(true, "Test group 29");

  keyIndex = 0;

  request = objectStore.index("name").openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      let nextKey = !keyIndex ? "Flo" : undefined;

      cursor.continue(nextKey);

      is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
         "Correct key");
      is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
         "Correct primary key");
      is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
         "Correct name");
      is(cursor.value.height,
         objectStoreDataNameSort[keyIndex].value.height,
         "Correct height");
      if ("weight" in cursor.value) {
        is(cursor.value.weight,
           objectStoreDataNameSort[keyIndex].value.weight,
           "Correct weight");
      }

      keyIndex += keyIndex ? 1 : 2;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, objectStoreDataNameSort.length, "Saw all the expected keys");

  finishTest();
}
