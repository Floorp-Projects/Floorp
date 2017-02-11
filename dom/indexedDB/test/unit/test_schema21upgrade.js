/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const testName = "schema21upgrade";
  const testKeys = [
    -1/0,
    -1.7e308,
    -10000,
    -2,
    -1.5,
    -1,
    -1.00001e-200,
    -1e-200,
    0,
    1e-200,
    1.00001e-200,
    1,
    2,
    10000,
    1.7e308,
    1/0,
    new Date("1750-01-02"),
    new Date("1800-12-31T12:34:56.001Z"),
    new Date(-1000),
    new Date(-10),
    new Date(-1),
    new Date(0),
    new Date(1),
    new Date(2),
    new Date(1000),
    new Date("1971-01-01"),
    new Date("1971-01-01T01:01:01Z"),
    new Date("1971-01-01T01:01:01.001Z"),
    new Date("1971-01-01T01:01:01.01Z"),
    new Date("1971-01-01T01:01:01.1Z"),
    new Date("1980-02-02"),
    new Date("3333-03-19T03:33:33.333Z"),
    "",
    "\x00",
    "\x00\x00",
    "\x00\x01",
    "\x01",
    "\x02",
    "\x03",
    "\x04",
    "\x07",
    "\x08",
    "\x0F",
    "\x10",
    "\x1F",
    "\x20",
    "01234",
    "\x3F",
    "\x40",
    "A",
    "A\x00",
    "A1",
    "ZZZZ",
    "a",
    "a\x00",
    "aa",
    "azz",
    "}",
    "\x7E",
    "\x7F",
    "\x80",
    "\xFF",
    "\u0100",
    "\u01FF",
    "\u0200",
    "\u03FF",
    "\u0400",
    "\u07FF",
    "\u0800",
    "\u0FFF",
    "\u1000",
    "\u1FFF",
    "\u2000",
    "\u3FFF",
    "\u4000",
    "\u7FFF",
    "\u8000",
    "\uD800",
    "\uD800a",
    "\uD800\uDC01",
    "\uDBFF",
    "\uDC00",
    "\uDFFF\uD800",
    "\uFFFE",
    "\uFFFF",
      "\uFFFF\x00",
    "\uFFFFZZZ",
    [],
    [-1/0],
    [-1],
    [0],
    [1],
    [1, "a"],
    [1, []],
    [1, [""]],
    [2, 3],
    [2, 3.0000000000001],
    [12, [[]]],
    [12, [[[]]]],
    [12, [[[""]]]],
    [12, [[["foo"]]]],
    [12, [[[[[3]]]]]],
    [12, [[[[[[3]]]]]]],
    [12, [[[[[[3],[[[[[4.2]]]]]]]]]]],
    [new Date(-1)],
    [new Date(1)],
    [""],
    ["", [[]]],
    ["", [[[]]]],
    ["abc"],
    ["abc", "def"],
    ["abc\x00"],
    ["abc\x00", "\x00\x01"],
    ["abc\x00", "\x00def"],
    ["abc\x00\x00def"],
    ["x", [[]]],
    ["x", [[[]]]],
    [[]],
    [[],"foo"],
    [[],[]],
    [[[]]],
    [[[]], []],
    [[[]], [[]]],
    [[[]], [[1]]],
    [[[]], [[[]]]],
    [[[1]]],
    [[[[]], []]],
  ];
  const testString =
    "abcdefghijklmnopqrstuvwxyz0123456789`~!@#$%^&*()-_+=,<.>/?\\|";

  clearAllDatabases(continueToNextStepSync);
  yield undefined;

  info("Installing profile");

  installPackagedProfile(testName + "_profile");

  info("Opening database with no version");

  let request = indexedDB.open(testName);
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;

  is(db.version, 1, "Correct db version");

  let transaction = db.transaction(testName);
  transaction.oncomplete = grabEventAndContinueHandler;

  let objectStore = transaction.objectStore(testName);
  let index = objectStore.index("uniqueIndex");

  info("Starting 'uniqueIndex' cursor");

  let keyIndex = 0;
  index.openCursor().onsuccess = event => {
    let cursor = event.target.result;
    if (cursor) {
      info("Comparing " + JSON.stringify(cursor.primaryKey) + " to " +
           JSON.stringify(testKeys[cursor.key]) +
           " [" + cursor.key + "]");
      is(indexedDB.cmp(cursor.primaryKey, testKeys[cursor.key]), 0,
         "Keys compare equally via 'indexedDB.cmp'");
      is(compareKeys(cursor.primaryKey, testKeys[cursor.key]), true,
         "Keys compare equally via 'compareKeys'");

      let indexProperty = cursor.value.index;
      is(Array.isArray(indexProperty), true, "index property is Array");
      is(indexProperty[0], cursor.key, "index property first item correct");
      is(indexProperty[1], cursor.key + 1, "index property second item correct");

      is(cursor.key, keyIndex, "Cursor key property is correct");

      is(cursor.value.testString, testString, "Test string compared equally");

      keyIndex++;
      cursor.continue();
    }
  };
  yield undefined;

  is(keyIndex, testKeys.length, "Saw all keys");

  transaction = db.transaction(testName, "readwrite");
  transaction.oncomplete = grabEventAndContinueHandler;

  objectStore = transaction.objectStore(testName);
  index = objectStore.index("index");

  info("Getting all 'index' keys");

  index.getAllKeys().onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result.length, testKeys.length * 2, "Got all keys");

  info("Starting objectStore cursor");

  objectStore.openCursor().onsuccess = event => {
    let cursor = event.target.result;
    if (cursor) {
      let value = cursor.value;
      is(value.testString, testString, "Test string compared equally");

      delete value.index;
      cursor.update(value);

      cursor.continue();
    } else {
      continueToNextStepSync();
    }
  };
  yield undefined;

  info("Getting all 'index' keys");

  index.getAllKeys().onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.target.result.length, 0, "Removed all keys");
  yield undefined;

  db.close();

  info("Opening database with new version");

  request = indexedDB.open(testName, 2);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  info("Deleting indexes");

  objectStore = event.target.transaction.objectStore(testName);
  objectStore.deleteIndex("index");
  objectStore.deleteIndex("uniqueIndex");

  event = yield undefined;

  db = event.target.result;

  transaction = db.transaction(testName, "readwrite");
  transaction.oncomplete = grabEventAndContinueHandler;

  info("Starting objectStore cursor");

  objectStore = transaction.objectStore(testName);
  objectStore.openCursor().onsuccess = event => {
    let cursor = event.target.result;
    if (cursor) {
      let value = cursor.value;
      is(value.testString, testString, "Test string compared equally");

      value.index = value.keyPath;
      cursor.update(value);

      cursor.continue();
    }
  };
  event = yield undefined;

  db.close();

  info("Opening database with new version");

  request = indexedDB.open(testName, 3);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  info("Creating indexes");

  objectStore = event.target.transaction.objectStore(testName);
  objectStore.createIndex("index", "index");

  event = yield undefined;

  db = event.target.result;

  transaction = db.transaction(testName);
  transaction.oncomplete = grabEventAndContinueHandler;

  objectStore = transaction.objectStore(testName);
  index = objectStore.index("index");

  info("Starting 'index' cursor");

  keyIndex = 0;
  index.openCursor().onsuccess = event => {
    let cursor = event.target.result;
    if (cursor) {
      is(indexedDB.cmp(cursor.primaryKey, testKeys[keyIndex]), 0,
         "Keys compare equally via 'indexedDB.cmp'");
      is(compareKeys(cursor.primaryKey, testKeys[keyIndex]), true,
         "Keys compare equally via 'compareKeys'");
      is(indexedDB.cmp(cursor.key, testKeys[keyIndex]), 0,
         "Keys compare equally via 'indexedDB.cmp'");
      is(compareKeys(cursor.key, testKeys[keyIndex]), true,
         "Keys compare equally via 'compareKeys'");

      let indexProperty = cursor.value.index;
      is(indexedDB.cmp(indexProperty, testKeys[keyIndex]), 0,
         "Keys compare equally via 'indexedDB.cmp'");
      is(compareKeys(indexProperty, testKeys[keyIndex]), true,
         "Keys compare equally via 'compareKeys'");

      is(cursor.value.testString, testString, "Test string compared equally");

      keyIndex++;
      cursor.continue();
    }
  };
  yield undefined;

  is(keyIndex, testKeys.length, "Added all keys again");

  finishTest();
  yield undefined;
}
