/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  let request = indexedDB.open(this.window ? window.location.pathname : "Splendid Test", 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = request.result;
  db.onerror = errorHandler;

  let objectStore = db.createObjectStore("foo", { keyPath: "id",
                                                  autoIncrement: true });
  objectStore.createIndex("first","first");
  objectStore.createIndex("second","second");
  objectStore.createIndex("third","third");

  let data = { first: "foo", second: "foo", third: "foo" };

  objectStore.add(data).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, 1, "Added entry");
  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  objectStore = db.transaction("foo").objectStore("foo");
  let first = objectStore.index("first");
  let second = objectStore.index("second");
  let third = objectStore.index("third");

  first.get("foo").onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is (event.target.result.id, 1, "Entry in first");

  second.get("foo").onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is (event.target.result.id, 1, "Entry in second");

  third.get("foo").onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is (event.target.result.id, 1, "Entry in third");

  finishTest();
}
