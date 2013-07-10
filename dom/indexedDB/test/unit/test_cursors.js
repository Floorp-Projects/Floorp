/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";
  const keys = [1, -1, 0, 10, 2000, "q", "z", "two", "b", "a"];
  const sortedKeys = [-1, 0, 1, 10, 2000, "a", "b", "q", "two", "z"];

  is(keys.length, sortedKeys.length, "Good key setup");

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;

  let objectStore = db.createObjectStore("autoIncrement",
                                         { autoIncrement: true });

  request = objectStore.openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    ok(!event.target.result, "No results");
    testGenerator.next();
  }
  yield undefined;

  objectStore = db.createObjectStore("autoIncrementKeyPath",
                                     { keyPath: "foo",
                                       autoIncrement: true });

  request = objectStore.openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    ok(!event.target.result, "No results");
    testGenerator.next();
  }
  yield undefined;

  objectStore = db.createObjectStore("keyPath", { keyPath: "foo" });

  request = objectStore.openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    ok(!event.target.result, "No results");
    testGenerator.next();
  }
  yield undefined;

  objectStore = db.createObjectStore("foo");

  request = objectStore.openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    ok(!event.target.result, "No results");
    testGenerator.next();
  }
  yield undefined;

  let keyIndex = 0;

  for (let i in keys) {
    request = objectStore.add("foo", keys[i]);
    request.onerror = errorHandler;
    request.onsuccess = function(event) {
      if (++keyIndex == keys.length) {
        testGenerator.next();
      }
    };
  }
  yield undefined;

  keyIndex = 0;

  request = objectStore.openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      cursor.continue();

      try {
        cursor.continue();
        ok(false, "continue twice should throw");
      }
      catch (e) {
        ok(e instanceof DOMException, "got a database exception");
        is(e.name, "InvalidStateError", "correct error");
        is(e.code, DOMException.INVALID_STATE_ERR, "correct code");
      }

      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, keys.length, "Saw all added items");

  keyIndex = 4;

  let range = IDBKeyRange.bound(2000, "q");
  request = objectStore.openCursor(range);
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      cursor.continue();

      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      keyIndex++;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, 8, "Saw all the expected keys");

  keyIndex = 0;

  request = objectStore.openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      if (keyIndex) {
        cursor.continue();
      }
      else {
        cursor.continue("b");
      }

      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      keyIndex += keyIndex ? 1: 6;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, keys.length, "Saw all the expected keys");

  keyIndex = 0;

  request = objectStore.openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      if (keyIndex) {
        cursor.continue();
      }
      else {
        cursor.continue(10);
      }

      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      keyIndex += keyIndex ? 1: 3;
    }
    else {
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, keys.length, "Saw all the expected keys");

  keyIndex = 0;

  request = objectStore.openCursor();
  request.onerror = errorHandler;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      if (keyIndex) {
        cursor.continue();
      }
      else {
        cursor.continue("c");
      }

      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      keyIndex += keyIndex ? 1 : 7;
    }
    else {
      ok(cursor === null, "The request result should be null.");
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, keys.length, "Saw all the expected keys");

  keyIndex = 0;

  request = objectStore.openCursor();
  request.onerror = errorHandler;
  let storedCursor = null;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      storedCursor = cursor;

      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      if (keyIndex == 4) {
        request = cursor.update("bar");
        request.onerror = errorHandler;
        request.onsuccess = function(event) {
          keyIndex++;
          cursor.continue();
        };
      }
      else {
        keyIndex++;
        cursor.continue();
      }
    }
    else {
      ok(cursor === null, "The request result should be null.");
      ok(storedCursor.value === undefined, "The cursor's value should be undefined.");
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, keys.length, "Saw all the expected keys");

  request = objectStore.get(sortedKeys[4]);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, "bar", "Update succeeded");

  request = objectStore.put("foo", sortedKeys[4]);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  keyIndex = 0;

  let gotRemoveEvent = false;
  let retval = false;

  request = objectStore.openCursor(null, "next");
  request.onerror = errorHandler;
  storedCursor = null;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      storedCursor = cursor;

      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      if (keyIndex == 4) {
        request = cursor.delete();
        request.onerror = errorHandler;
        request.onsuccess = function(event) {
          ok(event.target.result === undefined, "Should be undefined");
          is(keyIndex, 5, "Got result of remove before next continue");
          gotRemoveEvent = true;
        };
      }

      keyIndex++;
      cursor.continue();
    }
    else {
      ok(cursor === null, "The request result should be null.");
      ok(storedCursor.value === undefined, "The cursor's value should be undefined.");
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, keys.length, "Saw all the expected keys");
  is(gotRemoveEvent, true, "Saw the remove event");

  request = objectStore.get(sortedKeys[4]);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result, undefined, "Entry was deleted");

  request = objectStore.add("foo", sortedKeys[4]);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  keyIndex = sortedKeys.length - 1;

  request = objectStore.openCursor(null, "prev");
  request.onerror = errorHandler;
  storedCursor = null;
  request.onsuccess = function (event) {
    let cursor = event.target.result;
    if (cursor) {
      storedCursor = cursor;

      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      cursor.continue();

      is(cursor.key, sortedKeys[keyIndex], "Correct key");
      is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
      is(cursor.value, "foo", "Correct value");

      keyIndex--;
    }
    else {
      ok(cursor === null, "The request result should be null.");
      ok(storedCursor.value === undefined, "The cursor's value should be undefined.");
      testGenerator.next();
    }
  }
  yield undefined;

  is(keyIndex, -1, "Saw all added items");

  finishTest();
  yield undefined;
}
