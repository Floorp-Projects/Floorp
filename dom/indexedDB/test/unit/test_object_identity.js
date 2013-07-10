/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  let request = indexedDB.open(this.window ? window.location.pathname : "Splendid Test", 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  let transaction = event.target.transaction;

  let objectStore1 = db.createObjectStore("foo");
  let objectStore2 = transaction.objectStore("foo");
  ok(objectStore1 === objectStore2, "Got same objectStores");

  let index1 = objectStore1.createIndex("bar", "key");
  let index2 = objectStore2.index("bar");
  ok(index1 === index2, "Got same indexes");

  request.onsuccess = continueToNextStep;
  yield undefined;

  transaction = db.transaction(db.objectStoreNames);

  let objectStore3 = transaction.objectStore("foo");
  let objectStore4 = transaction.objectStore("foo");
  ok(objectStore3 === objectStore4, "Got same objectStores");

  ok(objectStore3 !== objectStore1, "Different objectStores");
  ok(objectStore4 !== objectStore2, "Different objectStores");

  let index3 = objectStore3.index("bar");
  let index4 = objectStore4.index("bar");
  ok(index3 === index4, "Got same indexes");

  ok(index3 !== index1, "Different indexes");
  ok(index4 !== index2, "Different indexes");

  finishTest();
  yield undefined;
}

