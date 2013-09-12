/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = "Splendid Test";
  const version = 1;

  const objectStoreName = "Foo";
  const data = { key: 1, value: "bar" };

  try {
    indexedDB.open(name, { version: version, storage: "unknown" });
    ok(false, "Should have thrown!");
  }
  catch (e) {
    ok(e instanceof TypeError, "Got TypeError.");
    is(e.name, "TypeError", "Good error name.");
  }

  let request = indexedDB.open(name, { version: version,
                                       storage: "persistent" });
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  is(event.type, "upgradeneeded", "Got correct event type");

  let db = event.target.result;
  db.onerror = errorHandler;

  let objectStore = db.createObjectStore(objectStoreName, { });

  event = yield undefined;

  is(event.type, "success", "Got correct event type");

  is(db.name, name, "Correct name");
  is(db.version, version, "Correct version");
  is(db.storage, "persistent", "Correct persistence type");

  objectStore = db.transaction([objectStoreName], "readwrite")
                  .objectStore(objectStoreName);

  request = objectStore.get(data.key);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, null, "Got no data");

  request = objectStore.add(data.value, data.key);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, data.key, "Got correct key");

  request = indexedDB.open(name, { version: version,
                                   storage: "temporary" });
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  is(event.type, "success", "Got correct event type");

  is(db.name, name, "Correct name");
  is(db.version, version, "Correct version");
  is(db.storage, "persistent", "Correct persistence type");

  objectStore = db.transaction([objectStoreName])
                  .objectStore(objectStoreName);

  request = objectStore.get(data.key);
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, data.value, "Got correct data");

  finishTest();
  yield undefined;
}
