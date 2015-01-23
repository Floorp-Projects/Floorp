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
  request.onsuccess = grabEventAndContinueHandler;

  let event = yield undefined;

  let db = event.target.result;

  for (let autoIncrement of [false, true]) {
    let objectStore =
      db.createObjectStore(autoIncrement, { keyPath: "id",
                                            autoIncrement: autoIncrement });
    objectStore.createIndex("", "index", { unique: true });

    for (let i = 0; i < 10; i++) {
      objectStore.add({ id: i, index: i });
    }
  }

  event = yield undefined;
  is(event.type, "success", "expect a success event");

  for (let autoIncrement of [false, true]) {
    objectStore = db.transaction(autoIncrement, "readwrite")
                    .objectStore(autoIncrement);

    request = objectStore.put({ id: 5, index: 6 });
    request.onsuccess = unexpectedSuccessHandler;
    request.addEventListener("error", new ExpectError("ConstraintError", true));
    event = yield undefined;

    event.preventDefault();

    let keyRange = IDBKeyRange.only(5);

    objectStore.index("").openCursor(keyRange).onsuccess = function(event) {
      let cursor = event.target.result;
      ok(cursor, "Must have a cursor here");

      is(cursor.value.index, 5, "Still have the right index value");

      cursor.value.index = 6;

      request = cursor.update(cursor.value);
      request.onsuccess = unexpectedSuccessHandler;
      request.addEventListener("error", new ExpectError("ConstraintError", true));
    };

    yield undefined;
  }

  finishTest();
  yield undefined;
}
