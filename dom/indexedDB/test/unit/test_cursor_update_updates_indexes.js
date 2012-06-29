/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const description = "My Test Database";
  const START_DATA = "hi";
  const END_DATA = "bye";
  const objectStoreInfo = [
    { name: "1", options: { keyPath: null }, key: 1,
      entry: { data: START_DATA } },
    { name: "2", options: { keyPath: "foo" },
      entry: { foo: 1, data: START_DATA } },
    { name: "3", options: { keyPath: null, autoIncrement: true },
      entry: { data: START_DATA } },
    { name: "4", options: { keyPath: "foo", autoIncrement: true },
      entry: { data: START_DATA } },
  ];

  for (let i = 0; i < objectStoreInfo.length; i++) {
    // Create our object stores.
    let info = objectStoreInfo[i];

    ok(true, "1");
    request = indexedDB.open(name, i + 1, description);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    event = yield;

    let db = event.target.result;

    ok(true, "2");
    let objectStore = info.hasOwnProperty("options") ?
                      db.createObjectStore(info.name, info.options) :
                      db.createObjectStore(info.name);

    // Create the indexes on 'data' on the object store.
    let index = objectStore.createIndex("data_index", "data",
                                        { unique: false });
    let uniqueIndex = objectStore.createIndex("unique_data_index", "data",
                                              { unique: true });
    // Populate the object store with one entry of data.
    request = info.hasOwnProperty("key") ?
              objectStore.add(info.entry, info.key) :
              objectStore.add(info.entry);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield;
    ok(true, "3");

    // Use a cursor to update 'data' to END_DATA.
    request = objectStore.openCursor();
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield;
    ok(true, "4");

    let cursor = request.result;
    let obj = cursor.value;
    obj.data = END_DATA;
    request = cursor.update(obj);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield;
    ok(true, "5");

    // Check both indexes to make sure that they were updated.
    request = index.get(END_DATA);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield;
    ok(true, "6");
    ok(obj.data, event.target.result.data,
                  "Non-unique index was properly updated.");

    request = uniqueIndex.get(END_DATA);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield;

    ok(true, "7");
    ok(obj.data, event.target.result.data,
                  "Unique index was properly updated.");
    db.close();
  }

  finishTest();
  yield;
}

