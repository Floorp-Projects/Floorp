/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var disableWorkerTest = "Need to implement a gc() function for worker tests";

if (!this.window) {
  this.runTest = function() {
    todo(false, "Test disabled in xpcshell test suite for now");
    finishTest();
  }
}

var testGenerator = testSteps();

function genCheck(key, value, test, options) {
  return function(event) {
    is(JSON.stringify(event.target.result), JSON.stringify(key),
       "correct returned key in " + test);
    if (options && options.store) {
      is(event.target.source, options.store, "correct store in " + test);
    }
    if (options && options.trans) {
      is(event.target.transaction, options.trans, "correct transaction in " + test);
    }

    event.target.source.get(key).onsuccess = function(event) {
      is(JSON.stringify(event.target.result), JSON.stringify(value),
         "correct stored value in " + test);
      continueToNextStepSync();
    }
  }
}

function* testSteps()
{
  const dbname = this.window ? window.location.pathname : "Splendid Test";
  const RW = "readwrite";
  let c1 = 1;
  let c2 = 1;

  let openRequest = indexedDB.open(dbname, 1);
  openRequest.onerror = errorHandler;
  openRequest.onupgradeneeded = grabEventAndContinueHandler;
  openRequest.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;
  let db = event.target.result;
  let trans = event.target.transaction;

  // Create test stores
  let store1 = db.createObjectStore("store1", { autoIncrement: true });
  let store2 = db.createObjectStore("store2", { autoIncrement: true, keyPath: "id" });
  let store3 = db.createObjectStore("store3", { autoIncrement: false });
  is(store1.autoIncrement, true, "store1 .autoIncrement");
  is(store2.autoIncrement, true, "store2 .autoIncrement");
  is(store3.autoIncrement, false, "store3 .autoIncrement");

  store1.createIndex("unique1", "unique", { unique: true });
  store2.createIndex("unique1", "unique", { unique: true });

  // Test simple inserts
  let test = " for test simple insert"
  store1.add({ foo: "value1" }).onsuccess =
    genCheck(c1++, { foo: "value1" }, "first" + test);
  store1.add({ foo: "value2" }).onsuccess =
    genCheck(c1++, { foo: "value2" }, "second" + test);

  yield undefined;
  yield undefined;

  store2.put({ bar: "value1" }).onsuccess =
    genCheck(c2, { bar: "value1", id: c2 }, "first in store2" + test,
             { store: store2 });
  c2++;
  store1.put({ foo: "value3" }).onsuccess =
    genCheck(c1++, { foo: "value3" }, "third" + test,
             { store: store1 });

  yield undefined;
  yield undefined;

  store2.get(IDBKeyRange.lowerBound(c2)).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;
  is(event.target.result, undefined, "no such value" + test);

  // Close version_change transaction
  openRequest.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target, openRequest, "succeeded to open" + test);
  is(event.type, "success", "succeeded to open" + test);

  // Test inserting explicit keys
  test = " for test explicit keys";
  trans = db.transaction("store1", RW);
  trans.objectStore("store1").add({ explicit: 1 }, 100).onsuccess =
    genCheck(100, { explicit: 1 }, "first" + test);
  c1 = 101;
  trans = db.transaction("store1", RW);
  trans.objectStore("store1").add({ explicit: 2 }).onsuccess =
    genCheck(c1++, { explicit: 2 }, "second" + test);
  yield undefined; yield undefined;

  trans = db.transaction("store1", RW);
  trans.objectStore("store1").add({ explicit: 3 }, 200).onsuccess =
    genCheck(200, { explicit: 3 }, "third" + test);
  c1 = 201;
  trans.objectStore("store1").add({ explicit: 4 }).onsuccess =
    genCheck(c1++, { explicit: 4 }, "fourth" + test);
  yield undefined; yield undefined;

  trans = db.transaction("store1", RW);
  trans.objectStore("store1").add({ explicit: 5 }, 150).onsuccess =
    genCheck(150, { explicit: 5 }, "fifth" + test);
  yield undefined;
  trans.objectStore("store1").add({ explicit: 6 }).onsuccess =
    genCheck(c1++, { explicit: 6 }, "sixth" + test);
  yield undefined;

  trans = db.transaction("store1", RW);
  trans.objectStore("store1").add({ explicit: 7 }, "key").onsuccess =
    genCheck("key", { explicit: 7 }, "seventh" + test);
  yield undefined;
  trans.objectStore("store1").add({ explicit: 8 }).onsuccess =
    genCheck(c1++, { explicit: 8 }, "eighth" + test);
  yield undefined;

  trans = db.transaction("store1", RW);
  trans.objectStore("store1").add({ explicit: 7 }, [100000]).onsuccess =
    genCheck([100000], { explicit: 7 }, "seventh" + test);
  yield undefined;
  trans.objectStore("store1").add({ explicit: 8 }).onsuccess =
    genCheck(c1++, { explicit: 8 }, "eighth" + test);
  yield undefined;

  trans = db.transaction("store1", RW);
  trans.objectStore("store1").add({ explicit: 9 }, -100000).onsuccess =
    genCheck(-100000, { explicit: 9 }, "ninth" + test);
  yield undefined;
  trans.objectStore("store1").add({ explicit: 10 }).onsuccess =
    genCheck(c1++, { explicit: 10 }, "tenth" + test);
  yield undefined;


  trans = db.transaction("store2", RW);
  trans.objectStore("store2").add({ explicit2: 1, id: 300 }).onsuccess =
    genCheck(300, { explicit2: 1, id: 300 }, "first store2" + test);
  c2 = 301;
  trans = db.transaction("store2", RW);
  trans.objectStore("store2").add({ explicit2: 2 }).onsuccess =
    genCheck(c2, { explicit2: 2, id: c2 }, "second store2" + test);
  c2++;
  yield undefined; yield undefined;

  trans = db.transaction("store2", RW);
  trans.objectStore("store2").add({ explicit2: 3, id: 400 }).onsuccess =
    genCheck(400, { explicit2: 3, id: 400 }, "third store2" + test);
  c2 = 401;
  trans.objectStore("store2").add({ explicit2: 4 }).onsuccess =
    genCheck(c2, { explicit2: 4, id: c2 }, "fourth store2" + test);
  c2++;
  yield undefined; yield undefined;

  trans = db.transaction("store2", RW);
  trans.objectStore("store2").add({ explicit: 5, id: 150 }).onsuccess =
    genCheck(150, { explicit: 5, id: 150 }, "fifth store2" + test);
  yield undefined;
  trans.objectStore("store2").add({ explicit: 6 }).onsuccess =
    genCheck(c2, { explicit: 6, id: c2 }, "sixth store2" + test);
  c2++;
  yield undefined;

  trans = db.transaction("store2", RW);
  trans.objectStore("store2").add({ explicit: 7, id: "key" }).onsuccess =
    genCheck("key", { explicit: 7, id: "key" }, "seventh store2" + test);
  yield undefined;
  trans.objectStore("store2").add({ explicit: 8 }).onsuccess =
    genCheck(c2, { explicit: 8, id: c2 }, "eighth store2" + test);
  c2++;
  yield undefined;

  trans = db.transaction("store2", RW);
  trans.objectStore("store2").add({ explicit: 7, id: [100000] }).onsuccess =
    genCheck([100000], { explicit: 7, id: [100000] }, "seventh store2" + test);
  yield undefined;
  trans.objectStore("store2").add({ explicit: 8 }).onsuccess =
    genCheck(c2, { explicit: 8, id: c2 }, "eighth store2" + test);
  c2++;
  yield undefined;

  trans = db.transaction("store2", RW);
  trans.objectStore("store2").add({ explicit: 9, id: -100000 }).onsuccess =
    genCheck(-100000, { explicit: 9, id: -100000 }, "ninth store2" + test);
  yield undefined;
  trans.objectStore("store2").add({ explicit: 10 }).onsuccess =
    genCheck(c2, { explicit: 10, id: c2 }, "tenth store2" + test);
  c2++;
  yield undefined;


  // Test separate transactions doesn't generate overlapping numbers
  test = " for test non-overlapping counts";
  trans = db.transaction("store1", RW);
  let trans2 = db.transaction("store1", RW);
  trans2.objectStore("store1").put({ over: 2 }).onsuccess =
    genCheck(c1 + 1, { over: 2 }, "first" + test,
             { trans: trans2 });
  trans.objectStore("store1").put({ over: 1 }).onsuccess =
    genCheck(c1, { over: 1 }, "second" + test,
             { trans });
  c1 += 2;
  yield undefined; yield undefined;

  trans = db.transaction("store2", RW);
  trans2 = db.transaction("store2", RW);
  trans2.objectStore("store2").put({ over: 2 }).onsuccess =
    genCheck(c2 + 1, { over: 2, id: c2 + 1 }, "third" + test,
             { trans: trans2 });
  trans.objectStore("store2").put({ over: 1 }).onsuccess =
    genCheck(c2, { over: 1, id: c2 }, "fourth" + test,
             { trans });
  c2 += 2;
  yield undefined; yield undefined;

  // Test that error inserts doesn't increase generator
  test = " for test error inserts";
  trans = db.transaction(["store1", "store2"], RW);
  trans.objectStore("store1").add({ unique: 1 }, -1);
  trans.objectStore("store2").add({ unique: 1, id: "unique" });

  trans.objectStore("store1").add({ error: 1, unique: 1 }).
    addEventListener("error", new ExpectError("ConstraintError", true));
  trans.objectStore("store1").add({ error: 2 }).onsuccess =
    genCheck(c1++, { error: 2 }, "first" + test);
  yield undefined; yield undefined;

  trans.objectStore("store2").add({ error: 3, unique: 1 }).
    addEventListener("error", new ExpectError("ConstraintError", true));
  trans.objectStore("store2").add({ error: 4 }).onsuccess =
    genCheck(c2, { error: 4, id: c2 }, "second" + test);
  c2++;
  yield undefined; yield undefined;

  trans.objectStore("store1").add({ error: 5, unique: 1 }, 100000).
    addEventListener("error", new ExpectError("ConstraintError", true));
  trans.objectStore("store1").add({ error: 6 }).onsuccess =
    genCheck(c1++, { error: 6 }, "third" + test);
  yield undefined; yield undefined;

  trans.objectStore("store2").add({ error: 7, unique: 1, id: 100000 }).
    addEventListener("error", new ExpectError("ConstraintError", true));
  trans.objectStore("store2").add({ error: 8 }).onsuccess =
    genCheck(c2, { error: 8, id: c2 }, "fourth" + test);
  c2++;
  yield undefined; yield undefined;

  // Test that aborts doesn't increase generator
  test = " for test aborted transaction";
  trans = db.transaction(["store1", "store2"], RW);
  trans.objectStore("store1").add({ abort: 1 }).onsuccess =
    genCheck(c1, { abort: 1 }, "first" + test);
  trans.objectStore("store2").put({ abort: 2 }).onsuccess =
    genCheck(c2, { abort: 2, id: c2 }, "second" + test);
  yield undefined; yield undefined;

  trans.objectStore("store1").add({ abort: 3 }, 500).onsuccess =
    genCheck(500, { abort: 3 }, "third" + test);
  trans.objectStore("store2").put({ abort: 4, id: 600 }).onsuccess =
    genCheck(600, { abort: 4, id: 600 }, "fourth" + test);
  yield undefined; yield undefined;

  trans.objectStore("store1").add({ abort: 5 }).onsuccess =
    genCheck(501, { abort: 5 }, "fifth" + test);
  trans.objectStore("store2").put({ abort: 6 }).onsuccess =
    genCheck(601, { abort: 6, id: 601 }, "sixth" + test);
  yield undefined; yield undefined;

  trans.abort();
  trans.onabort = grabEventAndContinueHandler;
  event = yield
  is(event.type, "abort", "transaction aborted");
  is(event.target, trans, "correct transaction aborted");

  trans = db.transaction(["store1", "store2"], RW);
  trans.objectStore("store1").add({ abort: 1 }).onsuccess =
    genCheck(c1++, { abort: 1 }, "re-first" + test);
  trans.objectStore("store2").put({ abort: 2 }).onsuccess =
    genCheck(c2, { abort: 2, id: c2 }, "re-second" + test);
  c2++;
  yield undefined; yield undefined;

  // Test that delete doesn't decrease generator
  test = " for test delete items"
  trans = db.transaction(["store1", "store2"], RW);
  trans.objectStore("store1").add({ delete: 1 }).onsuccess =
    genCheck(c1++, { delete: 1 }, "first" + test);
  trans.objectStore("store2").put({ delete: 2 }).onsuccess =
    genCheck(c2, { delete: 2, id: c2 }, "second" + test);
  c2++;
  yield undefined; yield undefined;

  trans.objectStore("store1").delete(c1 - 1).onsuccess =
    grabEventAndContinueHandler;
  trans.objectStore("store2").delete(c2 - 1).onsuccess =
    grabEventAndContinueHandler;
  yield undefined; yield undefined;

  trans.objectStore("store1").add({ delete: 3 }).onsuccess =
    genCheck(c1++, { delete: 3 }, "first" + test);
  trans.objectStore("store2").put({ delete: 4 }).onsuccess =
    genCheck(c2, { delete: 4, id: c2 }, "second" + test);
  c2++;
  yield undefined; yield undefined;

  trans.objectStore("store1").delete(c1 - 1).onsuccess =
    grabEventAndContinueHandler;
  trans.objectStore("store2").delete(c2 - 1).onsuccess =
    grabEventAndContinueHandler;
  yield undefined; yield undefined;

  trans = db.transaction(["store1", "store2"], RW);
  trans.objectStore("store1").add({ delete: 5 }).onsuccess =
    genCheck(c1++, { delete: 5 }, "first" + test);
  trans.objectStore("store2").put({ delete: 6 }).onsuccess =
    genCheck(c2, { delete: 6, id: c2 }, "second" + test);
  c2++;
  yield undefined; yield undefined;

  // Test that clears doesn't decrease generator
  test = " for test clear stores";
  trans = db.transaction(["store1", "store2"], RW);
  trans.objectStore("store1").add({ clear: 1 }).onsuccess =
    genCheck(c1++, { clear: 1 }, "first" + test);
  trans.objectStore("store2").put({ clear: 2 }).onsuccess =
    genCheck(c2, { clear: 2, id: c2 }, "second" + test);
  c2++;
  yield undefined; yield undefined;

  trans.objectStore("store1").clear().onsuccess =
    grabEventAndContinueHandler;
  trans.objectStore("store2").clear().onsuccess =
    grabEventAndContinueHandler;
  yield undefined; yield undefined;

  trans.objectStore("store1").add({ clear: 3 }).onsuccess =
    genCheck(c1++, { clear: 3 }, "third" + test);
  trans.objectStore("store2").put({ clear: 4 }).onsuccess =
    genCheck(c2, { clear: 4, id: c2 }, "forth" + test);
  c2++;
  yield undefined; yield undefined;

  trans.objectStore("store1").clear().onsuccess =
    grabEventAndContinueHandler;
  trans.objectStore("store2").clear().onsuccess =
    grabEventAndContinueHandler;
  yield undefined; yield undefined;

  trans = db.transaction(["store1", "store2"], RW);
  trans.objectStore("store1").add({ clear: 5 }).onsuccess =
    genCheck(c1++, { clear: 5 }, "fifth" + test);
  trans.objectStore("store2").put({ clear: 6 }).onsuccess =
    genCheck(c2, { clear: 6, id: c2 }, "sixth" + test);
  c2++;
  yield undefined; yield undefined;


  // Test that close/reopen doesn't decrease generator
  test = " for test clear stores";
  trans = db.transaction(["store1", "store2"], RW);
  trans.objectStore("store1").clear().onsuccess =
    grabEventAndContinueHandler;
  trans.objectStore("store2").clear().onsuccess =
    grabEventAndContinueHandler;
  yield undefined; yield undefined;
  db.close();

  gc();

  openRequest = indexedDB.open(dbname, 2);
  openRequest.onerror = errorHandler;
  openRequest.onupgradeneeded = grabEventAndContinueHandler;
  openRequest.onsuccess = unexpectedSuccessHandler;
  event = yield undefined;
  db = event.target.result;
  trans = event.target.transaction;

  trans.objectStore("store1").add({ reopen: 1 }).onsuccess =
    genCheck(c1++, { reopen: 1 }, "first" + test);
  trans.objectStore("store2").put({ reopen: 2 }).onsuccess =
    genCheck(c2, { reopen: 2, id: c2 }, "second" + test);
  c2++;
  yield undefined; yield undefined;

  openRequest.onsuccess = grabEventAndContinueHandler;
  yield undefined;

  finishTest();
}
