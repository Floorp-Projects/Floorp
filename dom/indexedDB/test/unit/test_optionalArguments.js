/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const osName = "people";
  const indexName = "weight";

  const data = [
    { ssn: "237-23-7732", name: "Bob", height: 60, weight: 120 },
    { ssn: "237-23-7733", name: "Ann", height: 52, weight: 110 },
    { ssn: "237-23-7734", name: "Ron", height: 73, weight: 180 },
    { ssn: "237-23-7735", name: "Sue", height: 58, weight: 130 },
    { ssn: "237-23-7736", name: "Joe", height: 65, weight: 150 },
    { ssn: "237-23-7737", name: "Pat", height: 65 },
    { ssn: "237-23-7738", name: "Mel", height: 66, weight: {} },
    { ssn: "237-23-7739", name: "Tom", height: 62, weight: 130 }
  ];

  const weightSort = [1, 0, 3, 7, 4, 2];

  let request = mozIndexedDB.open(this.window ? window.location.pathname : "Splendid Test", 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield;

  is(event.type, "upgradeneeded", "Got upgradeneeded event");

  let db = event.target.result;
  db.onerror = errorHandler;

  let objectStore = db.createObjectStore(osName, { keyPath: "ssn" });
  objectStore.createIndex(indexName, "weight", { unique: false });

  for each (let i in data) {
    objectStore.add(i);
  }

  event = yield;

  is(event.type, "success", "Got success event");

  try {
    IDBKeyRange.bound(1, -1);
    ok(false, "Bound keyRange with backwards args should throw!");
  }
  catch (e) {
    is(e.name, "DataError", "Threw correct exception");
    is(e.code, 0, "Threw with correct code");
  }

  try {
    IDBKeyRange.bound(1, 1);
    ok(true, "Bound keyRange with same arg should be ok");
  }
  catch (e) {
    ok(false, "Bound keyRange with same arg should have been ok");
  }

  try {
    IDBKeyRange.bound(1, 1, true);
    ok(false, "Bound keyRange with same arg and open should throw!");
  }
  catch (e) {
    is(e.name, "DataError", "Threw correct exception");
    is(e.code, 0, "Threw with correct code");
  }

  try {
    IDBKeyRange.bound(1, 1, true, true);
    ok(false, "Bound keyRange with same arg and open should throw!");
  }
  catch (e) {
    is(e.name, "DataError", "Threw correct exception");
    is(e.code, 0, "Threw with correct code");
  }

  objectStore = db.transaction(osName).objectStore(osName);

  try {
    objectStore.get();
    ok(false, "Get with unspecified arg should have thrown");
  }
  catch(e) {
    ok(true, "Get with unspecified arg should have thrown");
  }

  try {
    objectStore.get(undefined);
    ok(false, "Get with undefined should have thrown");
  }
  catch(e) {
    ok(true, "Get with undefined arg should have thrown");
  }

  try {
    objectStore.get(null);
    ok(false, "Get with null should have thrown");
  }
  catch(e) {
    is(e instanceof DOMException, true,
       "Got right kind of exception");
    is(e.name, "DataError", "Correct error.");
    is(e.code, 0, "Correct code.");
  }

  objectStore.get(data[2].ssn).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.name, data[2].name, "Correct data");

  let keyRange = IDBKeyRange.only(data[2].ssn);

  objectStore.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.name, data[2].name, "Correct data");

  keyRange = IDBKeyRange.lowerBound(data[2].ssn);

  objectStore.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.name, data[2].name, "Correct data");

  keyRange = IDBKeyRange.lowerBound(data[2].ssn, true);

  objectStore.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.name, data[3].name, "Correct data");

  keyRange = IDBKeyRange.upperBound(data[2].ssn);

  objectStore.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.name, data[0].name, "Correct data");

  keyRange = IDBKeyRange.bound(data[2].ssn, data[4].ssn);

  objectStore.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.name, data[2].name, "Correct data");

  keyRange = IDBKeyRange.bound(data[2].ssn, data[4].ssn, true);

  objectStore.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.name, data[3].name, "Correct data");

  objectStore = db.transaction(osName, "readwrite")
                  .objectStore(osName);

  try {
    objectStore.delete();
    ok(false, "Delete with unspecified arg should have thrown");
  }
  catch(e) {
    ok(true, "Delete with unspecified arg should have thrown");
  }

  try {
    objectStore.delete(undefined);
    ok(false, "Delete with undefined should have thrown");
  }
  catch(e) {
    ok(true, "Delete with undefined arg should have thrown");
  }

  try {
    objectStore.delete(null);
    ok(false, "Delete with null should have thrown");
  }
  catch(e) {
    is(e instanceof DOMException, true,
       "Got right kind of exception");
    is(e.name, "DataError", "Correct error.");
    is(e.code, 0, "Correct code.");
  }

  objectStore.count().onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, data.length, "Correct count");

  objectStore.delete(data[2].ssn).onsuccess = grabEventAndContinueHandler;
  event = yield;

  ok(event.target.result === undefined, "Correct result");

  objectStore.count().onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, data.length - 1, "Correct count");

  keyRange = IDBKeyRange.bound(data[3].ssn, data[5].ssn);

  objectStore.delete(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  ok(event.target.result === undefined, "Correct result");

  objectStore.count().onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, data.length - 4, "Correct count");

  keyRange = IDBKeyRange.lowerBound(10);

  objectStore.delete(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  ok(event.target.result === undefined, "Correct result");

  objectStore.count().onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, 0, "Correct count");

  event.target.transaction.oncomplete = grabEventAndContinueHandler;

  for each (let i in data) {
    objectStore.add(i);
  }

  yield;

  objectStore = db.transaction(osName).objectStore(osName);

  objectStore.count().onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, data.length, "Correct count");

  let count = 0;

  objectStore.openCursor().onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, data.length, "Correct count for no arg to openCursor");

  count = 0;

  objectStore.openCursor(null).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, data.length, "Correct count for null arg to openCursor");

  count = 0;

  objectStore.openCursor(undefined).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, data.length, "Correct count for undefined arg to openCursor");

  count = 0;

  objectStore.openCursor(data[2].ssn).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 1, "Correct count for single key arg to openCursor");

  count = 0;

  objectStore.openCursor("foo").onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for non-existent single key arg to openCursor");

  count = 0;
  keyRange = IDBKeyRange.only(data[2].ssn);

  objectStore.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 1, "Correct count for only keyRange arg to openCursor");

  count = 0;
  keyRange = IDBKeyRange.lowerBound(data[2].ssn);

  objectStore.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, data.length - 2,
     "Correct count for lowerBound arg to openCursor");

  count = 0;
  keyRange = IDBKeyRange.lowerBound(data[2].ssn, true);

  objectStore.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, data.length - 3,
     "Correct count for lowerBound arg to openCursor");

  count = 0;
  keyRange = IDBKeyRange.lowerBound("foo");

  objectStore.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for non-existent lowerBound arg to openCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[2].ssn, data[3].ssn);

  objectStore.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 2, "Correct count for bound arg to openCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[2].ssn, data[3].ssn, true);

  objectStore.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 1, "Correct count for bound arg to openCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[2].ssn, data[3].ssn, true, true);

  objectStore.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0, "Correct count for bound arg to openCursor");

  let index = objectStore.index(indexName);

  count = 0;

  index.openKeyCursor().onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for unspecified arg to index.openKeyCursor");

  count = 0;

  index.openKeyCursor(null).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for null arg to index.openKeyCursor");

  count = 0;

  index.openKeyCursor(undefined).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for undefined arg to index.openKeyCursor");

  count = 0;

  index.openKeyCursor(data[0].weight).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 1, "Correct count for single key arg to index.openKeyCursor");

  count = 0;

  index.openKeyCursor("foo").onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for non-existent key arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.only("foo");

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for non-existent keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.only(data[0].weight);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 1,
     "Correct count for only keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for lowerBound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight, true);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length - 1,
     "Correct count for lowerBound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.lowerBound("foo");

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for lowerBound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.upperBound(data[weightSort[0]].weight);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 1,
     "Correct count for upperBound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.upperBound(data[weightSort[0]].weight, true);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for upperBound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.upperBound(data[weightSort[weightSort.length - 1]].weight);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for upperBound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.upperBound(data[weightSort[weightSort.length - 1]].weight,
                                    true);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length - 1,
     "Correct count for upperBound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.upperBound("foo");

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for upperBound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.upperBound(0);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for upperBound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                               data[weightSort[weightSort.length - 1]].weight);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for bound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                               data[weightSort[weightSort.length - 1]].weight,
                               true);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length - 1,
     "Correct count for bound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                               data[weightSort[weightSort.length - 1]].weight,
                               true, true);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length - 2,
     "Correct count for bound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight - 1,
                               data[weightSort[weightSort.length - 1]].weight + 1);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for bound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight - 2,
                               data[weightSort[0]].weight - 1);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for bound keyRange arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[weightSort[1]].weight,
                               data[weightSort[2]].weight);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 3,
     "Correct count for bound keyRange arg to index.openKeyCursor");

  count = 0;

  index.openCursor().onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for unspecified arg to index.openCursor");

  count = 0;

  index.openCursor(null).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for null arg to index.openCursor");

  count = 0;

  index.openCursor(undefined).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for undefined arg to index.openCursor");

  count = 0;

  index.openCursor(data[0].weight).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 1, "Correct count for single key arg to index.openCursor");

  count = 0;

  index.openCursor("foo").onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for non-existent key arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.only("foo");

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for non-existent keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.only(data[0].weight);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 1,
     "Correct count for only keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for lowerBound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight, true);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length - 1,
     "Correct count for lowerBound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.lowerBound("foo");

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for lowerBound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.upperBound(data[weightSort[0]].weight);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 1,
     "Correct count for upperBound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.upperBound(data[weightSort[0]].weight, true);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for upperBound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.upperBound(data[weightSort[weightSort.length - 1]].weight);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for upperBound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.upperBound(data[weightSort[weightSort.length - 1]].weight,
                                    true);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length - 1,
     "Correct count for upperBound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.upperBound("foo");

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for upperBound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.upperBound(0);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for upperBound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                               data[weightSort[weightSort.length - 1]].weight);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for bound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                               data[weightSort[weightSort.length - 1]].weight,
                               true);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length - 1,
     "Correct count for bound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                               data[weightSort[weightSort.length - 1]].weight,
                               true, true);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length - 2,
     "Correct count for bound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight - 1,
                               data[weightSort[weightSort.length - 1]].weight + 1);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for bound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight - 2,
                               data[weightSort[0]].weight - 1);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for bound keyRange arg to index.openCursor");

  count = 0;
  keyRange = IDBKeyRange.bound(data[weightSort[1]].weight,
                               data[weightSort[2]].weight);

  index.openCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 3,
     "Correct count for bound keyRange arg to index.openCursor");

  try {
    index.get();
    ok(false, "Get with unspecified arg should have thrown");
  }
  catch(e) {
    ok(true, "Get with unspecified arg should have thrown");
  }

  try {
    index.get(undefined);
    ok(false, "Get with undefined should have thrown");
  }
  catch(e) {
    ok(true, "Get with undefined arg should have thrown");
  }

  try {
    index.get(null);
    ok(false, "Get with null should have thrown");
  }
  catch(e) {
    is(e instanceof DOMException, true,
       "Got right kind of exception");
    is(e.name, "DataError", "Correct error.");
    is(e.code, 0, "Correct code.");
  }

  index.get(data[0].weight).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.weight, data[0].weight, "Got correct result");

  keyRange = IDBKeyRange.only(data[0].weight);

  index.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.weight, data[0].weight, "Got correct result");

  keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight);

  index.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.weight, data[weightSort[0]].weight,
     "Got correct result");

  keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight - 1);

  index.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.weight, data[weightSort[0]].weight,
     "Got correct result");

  keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight + 1);

  index.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.weight, data[weightSort[1]].weight,
     "Got correct result");

  keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight, true);

  index.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.weight, data[weightSort[1]].weight,
     "Got correct result");

  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                               data[weightSort[1]].weight);

  index.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.weight, data[weightSort[0]].weight,
     "Got correct result");

  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                               data[weightSort[1]].weight, true);

  index.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.weight, data[weightSort[1]].weight,
     "Got correct result");

  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                               data[weightSort[1]].weight, true, true);

  index.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, undefined, "Got correct result");

  keyRange = IDBKeyRange.upperBound(data[weightSort[5]].weight);

  index.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result.weight, data[weightSort[0]].weight,
     "Got correct result");

  keyRange = IDBKeyRange.upperBound(data[weightSort[0]].weight, true);

  index.get(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, undefined, "Got correct result");

  try {
    index.getKey();
    ok(false, "Get with unspecified arg should have thrown");
  }
  catch(e) {
    ok(true, "Get with unspecified arg should have thrown");
  }

  try {
    index.getKey(undefined);
    ok(false, "Get with undefined should have thrown");
  }
  catch(e) {
    ok(true, "Get with undefined arg should have thrown");
  }

  try {
    index.getKey(null);
    ok(false, "Get with null should have thrown");
  }
  catch(e) {
    is(e instanceof DOMException, true,
       "Got right kind of exception");
    is(e.name, "DataError", "Correct error.");
    is(e.code, 0, "Correct code.");
  }

  index.getKey(data[0].weight).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, data[0].ssn, "Got correct result");

  keyRange = IDBKeyRange.only(data[0].weight);

  index.getKey(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, data[0].ssn, "Got correct result");

  keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight);

  index.getKey(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, data[weightSort[0]].ssn, "Got correct result");

  keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight - 1);

  index.getKey(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, data[weightSort[0]].ssn, "Got correct result");

  keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight + 1);

  index.getKey(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, data[weightSort[1]].ssn, "Got correct result");

  keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight, true);

  index.getKey(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, data[weightSort[1]].ssn, "Got correct result");

  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                               data[weightSort[1]].weight);

  index.getKey(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, data[weightSort[0]].ssn, "Got correct result");

  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                               data[weightSort[1]].weight, true);

  index.getKey(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, data[weightSort[1]].ssn, "Got correct result");

  keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                               data[weightSort[1]].weight, true, true);

  index.getKey(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, undefined, "Got correct result");

  keyRange = IDBKeyRange.upperBound(data[weightSort[5]].weight);

  index.getKey(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, data[weightSort[0]].ssn, "Got correct result");

  keyRange = IDBKeyRange.upperBound(data[weightSort[0]].weight, true);

  index.getKey(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result, undefined, "Got correct result");

  count = 0;

  index.openKeyCursor().onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for no arg to index.openKeyCursor");

  count = 0;

  index.openKeyCursor(null).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for null arg to index.openKeyCursor");

  count = 0;

  index.openKeyCursor(undefined).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, weightSort.length,
     "Correct count for undefined arg to index.openKeyCursor");

  count = 0;

  index.openKeyCursor(data[weightSort[0]].weight).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 1, "Correct count for single key arg to index.openKeyCursor");

  count = 0;

  index.openKeyCursor("foo").onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 0,
     "Correct count for non-existent single key arg to index.openKeyCursor");

  count = 0;
  keyRange = IDBKeyRange.only(data[weightSort[0]].weight);

  index.openKeyCursor(keyRange).onsuccess = function(event) {
    let cursor = event.target.result;
    if (cursor) {
      count++;
      cursor.continue();
    }
    else {
      testGenerator.next();
    }
  }
  yield;

  is(count, 1,
     "Correct count for only keyRange arg to index.openKeyCursor");

  objectStore.getAll(data[1].ssn).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, 1, "Got correct length");
  is(event.target.result[0].ssn, data[1].ssn, "Got correct result");

  objectStore.getAll(null).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, data.length, "Got correct length");
  for (let i in event.target.result) {
    is(event.target.result[i].ssn, data[i].ssn, "Got correct value");
  }

  objectStore.getAll(undefined).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, data.length, "Got correct length");
  for (let i in event.target.result) {
    is(event.target.result[i].ssn, data[i].ssn, "Got correct value");
  }

  objectStore.getAll().onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, data.length, "Got correct length");
  for (let i in event.target.result) {
    is(event.target.result[i].ssn, data[i].ssn, "Got correct value");
  }

  keyRange = IDBKeyRange.lowerBound(0);

  objectStore.getAll(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, data.length, "Got correct length");
  for (let i in event.target.result) {
    is(event.target.result[i].ssn, data[i].ssn, "Got correct value");
  }

  index.getAll().onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, weightSort.length, "Got correct length");
  for (let i in event.target.result) {
    is(event.target.result[i].ssn, data[weightSort[i]].ssn,
       "Got correct value");
  }

  index.getAll(undefined).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, weightSort.length, "Got correct length");
  for (let i in event.target.result) {
    is(event.target.result[i].ssn, data[weightSort[i]].ssn,
       "Got correct value");
  }

  index.getAll(null).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, weightSort.length, "Got correct length");
  for (let i in event.target.result) {
    is(event.target.result[i].ssn, data[weightSort[i]].ssn,
       "Got correct value");
  }

  index.getAll(data[weightSort[0]].weight).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, 1, "Got correct length");
  is(event.target.result[0].ssn, data[weightSort[0]].ssn, "Got correct result");

  keyRange = IDBKeyRange.lowerBound(0);

  index.getAll(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, weightSort.length, "Got correct length");
  for (let i in event.target.result) {
    is(event.target.result[i].ssn, data[weightSort[i]].ssn,
       "Got correct value");
  }

  index.getAllKeys().onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, weightSort.length, "Got correct length");
  for (let i in event.target.result) {
    is(event.target.result[i], data[weightSort[i]].ssn,
       "Got correct value");
  }

  index.getAllKeys(undefined).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, weightSort.length, "Got correct length");
  for (let i in event.target.result) {
    is(event.target.result[i], data[weightSort[i]].ssn,
       "Got correct value");
  }

  index.getAllKeys(null).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, weightSort.length, "Got correct length");
  for (let i in event.target.result) {
    is(event.target.result[i], data[weightSort[i]].ssn,
       "Got correct value");
  }

  index.getAllKeys(data[weightSort[0]].weight).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, 1, "Got correct length");
  is(event.target.result[0], data[weightSort[0]].ssn, "Got correct result");

  keyRange = IDBKeyRange.lowerBound(0);

  index.getAllKeys(keyRange).onsuccess = grabEventAndContinueHandler;
  event = yield;

  is(event.target.result instanceof Array, true, "Got an array");
  is(event.target.result.length, weightSort.length, "Got correct length");
  for (let i in event.target.result) {
    is(event.target.result[i], data[weightSort[i]].ssn,
       "Got correct value");
  }

  finishTest();
  yield;
}

