/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const indexName = "My Test Index";

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  is(db.objectStoreNames.length, 0, "Correct objectStoreNames list");

  let objectStore = db.createObjectStore("test store", { keyPath: "foo" });
  is(db.objectStoreNames.length, 1, "Correct objectStoreNames list");
  is(db.objectStoreNames.item(0), objectStore.name, "Correct name");

  is(objectStore.indexNames.length, 0, "Correct indexNames list");

  let index = objectStore.createIndex(indexName, "foo");

  is(objectStore.indexNames.length, 1, "Correct indexNames list");
  is(objectStore.indexNames.item(0), indexName, "Correct name");
  is(objectStore.index(indexName), index, "Correct instance");

  objectStore.deleteIndex(indexName);

  is(objectStore.indexNames.length, 0, "Correct indexNames list");
  try {
    objectStore.index(indexName);
    ok(false, "should have thrown");
  }
  catch(ex) {
    ok(ex instanceof DOMException, "Got a DOMException");
    is(ex.name, "NotFoundError", "expect a NotFoundError");
    is(ex.code, DOMException.NOT_FOUND_ERR, "expect a NOT_FOUND_ERR");
  }

  let index2 = objectStore.createIndex(indexName, "foo");
  isnot(index, index2, "New instance should be created");

  is(objectStore.indexNames.length, 1, "Correct recreacted indexNames list");
  is(objectStore.indexNames.item(0), indexName, "Correct recreacted name");
  is(objectStore.index(indexName), index2, "Correct instance");

  finishTest();
  yield undefined;
}
