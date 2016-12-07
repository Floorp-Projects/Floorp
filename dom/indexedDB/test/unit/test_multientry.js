/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  // Test object stores
  
  let name = this.window ? window.location.pathname : "Splendid Test";
  let openRequest = indexedDB.open(name, 1);
  openRequest.onerror = errorHandler;
  openRequest.onupgradeneeded = grabEventAndContinueHandler;
  openRequest.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;
  let db = event.target.result;
  db.onerror = errorHandler;
  let tests =
    [{ add:     { x: 1, id: 1 },
       indexes:[{ v: 1, k: 1 }] },
     { add:     { x: [2, 3], id: 2 },
       indexes:[{ v: 1, k: 1 },
                { v: 2, k: 2 },
                { v: 3, k: 2 }] },
     { put:     { x: [2, 4], id: 1 },
       indexes:[{ v: 2, k: 1 },
                { v: 2, k: 2 },
                { v: 3, k: 2 },
                { v: 4, k: 1 }] },
     { add:     { x: [5, 6, 5, -2, 3], id: 3 },
       indexes:[{ v:-2, k: 3 },
                { v: 2, k: 1 },
                { v: 2, k: 2 },
                { v: 3, k: 2 },
                { v: 3, k: 3 },
                { v: 4, k: 1 },
                { v: 5, k: 3 },
                { v: 6, k: 3 }] },
     { delete:  IDBKeyRange.bound(1, 3),
       indexes:[] },
     { put:     { x: ["food", {}, false, undefined, /x/, [73, false]], id: 2 },
       indexes:[{ v: "food", k: 2 }] },
     { add:     { x: [{}, /x/, -12, "food", null, [false], undefined], id: 3 },
       indexes:[{ v: -12, k: 3 },
                { v: "food", k: 2 },
                { v: "food", k: 3 }] },
     { put:     { x: [], id: 2 },
       indexes:[{ v: -12, k: 3 },
                { v: "food", k: 3 }] },
     { put:     { x: { y: 3 }, id: 3 },
       indexes:[] },
     { add:     { x: false, id: 7 },
       indexes:[] },
     { delete:  IDBKeyRange.lowerBound(0),
       indexes:[] },
    ];

  let store = db.createObjectStore("mystore", { keyPath: "id" });
  let index = store.createIndex("myindex", "x", { multiEntry: true });
  is(index.multiEntry, true, "index created with multiEntry");

  let i;
  for (i = 0; i < tests.length; ++i) {
    let test = tests[i];
    let testName = " for " + JSON.stringify(test);
    let req;
    if (test.add) {
      req = store.add(test.add);
    }
    else if (test.put) {
      req = store.put(test.put);
    }
    else if (test.delete) {
      req = store.delete(test.delete);
    }
    else {
      ok(false, "borked test");
    }
    req.onsuccess = grabEventAndContinueHandler;
    let e = yield undefined;
    
    req = index.openKeyCursor();
    req.onsuccess = grabEventAndContinueHandler;
    for (let j = 0; j < test.indexes.length; ++j) {
      e = yield undefined;
      is(req.result.key, test.indexes[j].v, "found expected index key at index " + j + testName);
      is(req.result.primaryKey, test.indexes[j].k, "found expected index primary key at index " + j + testName);
      req.result.continue();
    }
    e = yield undefined;
    ok(req.result == null, "exhausted indexes");

    let tempIndex = store.createIndex("temp index", "x", { multiEntry: true });
    req = tempIndex.openKeyCursor();
    req.onsuccess = grabEventAndContinueHandler;
    for (let j = 0; j < test.indexes.length; ++j) {
      e = yield undefined;
      is(req.result.key, test.indexes[j].v, "found expected temp index key at index " + j + testName);
      is(req.result.primaryKey, test.indexes[j].k, "found expected temp index primary key at index " + j + testName);
      req.result.continue();
    }
    e = yield undefined;
    ok(req.result == null, "exhausted temp index");
    store.deleteIndex("temp index");
  }

  // Unique indexes
  tests =
    [{ add:     { x: 1, id: 1 },
       indexes:[{ v: 1, k: 1 }] },
     { add:     { x: [2, 3], id: 2 },
       indexes:[{ v: 1, k: 1 },
                { v: 2, k: 2 },
                { v: 3, k: 2 }] },
     { put:     { x: [2, 4], id: 3 },
       fail:    true },
     { put:     { x: [1, 4], id: 1 },
       indexes:[{ v: 1, k: 1 },
                { v: 2, k: 2 },
                { v: 3, k: 2 },
                { v: 4, k: 1 }] },
     { add:     { x: [5, 0, 5, 5, 5], id: 3 },
       indexes:[{ v: 0, k: 3 },
                { v: 1, k: 1 },
                { v: 2, k: 2 },
                { v: 3, k: 2 },
                { v: 4, k: 1 },
                { v: 5, k: 3 }] },
     { delete:  IDBKeyRange.bound(1, 2),
       indexes:[{ v: 0, k: 3 },
                { v: 5, k: 3 }] },
     { add:     { x: [0, 6], id: 8 },
       fail:    true },
     { add:     { x: 5, id: 8 },
       fail:    true },
     { put:     { x: 0, id: 8 },
       fail:    true },
    ];

  store.deleteIndex("myindex");
  index = store.createIndex("myindex", "x", { multiEntry: true, unique: true });
  is(index.multiEntry, true, "index created with multiEntry");

  let indexes;
  for (i = 0; i < tests.length; ++i) {
    let test = tests[i];
    let testName = " for " + JSON.stringify(test);
    let req;
    if (test.add) {
      req = store.add(test.add);
    }
    else if (test.put) {
      req = store.put(test.put);
    }
    else if (test.delete) {
      req = store.delete(test.delete);
    }
    else {
      ok(false, "borked test");
    }
    
    if (!test.fail) {
      req.onsuccess = grabEventAndContinueHandler;
      let e = yield undefined;
      indexes = test.indexes;
    }
    else {
      req.onsuccess = unexpectedSuccessHandler;
      req.onerror = grabEventAndContinueHandler;
      ok(true, "waiting for error");
      let e = yield undefined;
      ok(true, "got error: " + e.type);
      e.preventDefault();
      e.stopPropagation();
    }

    let e;
    req = index.openKeyCursor();
    req.onsuccess = grabEventAndContinueHandler;
    for (let j = 0; j < indexes.length; ++j) {
      e = yield undefined;
      is(req.result.key, indexes[j].v, "found expected index key at index " + j + testName);
      is(req.result.primaryKey, indexes[j].k, "found expected index primary key at index " + j + testName);
      req.result.continue();
    }
    e = yield undefined;
    ok(req.result == null, "exhausted indexes");

    let tempIndex = store.createIndex("temp index", "x", { multiEntry: true, unique: true });
    req = tempIndex.openKeyCursor();
    req.onsuccess = grabEventAndContinueHandler;
    for (let j = 0; j < indexes.length; ++j) {
      e = yield undefined;
      is(req.result.key, indexes[j].v, "found expected temp index key at index " + j + testName);
      is(req.result.primaryKey, indexes[j].k, "found expected temp index primary key at index " + j + testName);
      req.result.continue();
    }
    e = yield undefined;
    ok(req.result == null, "exhausted temp index");
    store.deleteIndex("temp index");
  }


  openRequest.onsuccess = grabEventAndContinueHandler;
  yield undefined;

  let trans = db.transaction(["mystore"], "readwrite");
  store = trans.objectStore("mystore");
  index = store.index("myindex");
  is(index.multiEntry, true, "index still is multiEntry");
  trans.oncomplete = grabEventAndContinueHandler;
  yield undefined;

  finishTest();
}
