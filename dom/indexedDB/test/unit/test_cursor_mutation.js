/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps() {
  const objectStoreData = [
    // This one will be removed.
    { ss: "237-23-7732", name: "Bob" },

    // These will always be included.
    { ss: "237-23-7733", name: "Ann" },
    { ss: "237-23-7734", name: "Ron" },
    { ss: "237-23-7735", name: "Sue" },
    { ss: "237-23-7736", name: "Joe" },

    // This one will be added.
    { ss: "237-23-7737", name: "Pat" },
  ];

  // Post-add and post-remove data ordered by name.
  const objectStoreDataNameSort = [1, 4, 5, 2, 3];

  let request = indexedDB.open(
    this.window ? window.location.pathname : "Splendid Test",
    1
  );
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  event.target.onsuccess = continueToNextStep;

  let objectStore = db.createObjectStore("foo", { keyPath: "ss" });
  objectStore.createIndex("name", "name", { unique: true });

  for (let i = 0; i < objectStoreData.length - 1; i++) {
    objectStore.add(objectStoreData[i]);
  }
  yield undefined;

  let count = 0;

  let sawAdded = false;
  let sawRemoved = false;

  db
    .transaction("foo")
    .objectStore("foo")
    .openCursor().onsuccess = function(event) {
    event.target.transaction.oncomplete = continueToNextStep;
    let cursor = event.target.result;
    if (cursor) {
      if (cursor.value.name == objectStoreData[0].name) {
        sawRemoved = true;
      }
      if (
        cursor.value.name == objectStoreData[objectStoreData.length - 1].name
      ) {
        sawAdded = true;
      }
      cursor.continue();
      count++;
    }
  };
  yield undefined;

  is(count, objectStoreData.length - 1, "Good initial count");
  is(sawAdded, false, "Didn't see item that is about to be added");
  is(sawRemoved, true, "Saw item that is about to be removed");

  count = 0;
  sawAdded = false;
  sawRemoved = false;

  db
    .transaction("foo", "readwrite")
    .objectStore("foo")
    .index("name")
    .openCursor().onsuccess = function(event) {
    event.target.transaction.oncomplete = continueToNextStep;
    let cursor = event.target.result;
    if (cursor) {
      if (cursor.value.name == objectStoreData[0].name) {
        sawRemoved = true;
      }
      if (
        cursor.value.name == objectStoreData[objectStoreData.length - 1].name
      ) {
        sawAdded = true;
      }

      is(
        cursor.value.name,
        objectStoreData[objectStoreDataNameSort[count++]].name,
        "Correct name"
      );

      if (count == 1) {
        let objectStore = event.target.transaction.objectStore("foo");
        objectStore.delete(objectStoreData[0].ss).onsuccess = function(event) {
          objectStore.add(
            objectStoreData[objectStoreData.length - 1]
          ).onsuccess = function(event) {
            cursor.continue();
          };
        };
      } else {
        cursor.continue();
      }
    }
  };
  yield undefined;

  is(count, objectStoreData.length - 1, "Good final count");
  is(sawAdded, true, "Saw item that was added");
  is(sawRemoved, false, "Didn't see item that was removed");

  finishTest();

  objectStore = null; // Bug 943409 workaround.
}
