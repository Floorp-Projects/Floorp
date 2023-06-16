/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testGenerator */
var testGenerator = testSteps();

// helper function that ensures that ArrayBuffer instances are meaningfully
// displayed (not just as 'object ArrayBuffer')
// TODO better move to helpers.js?
function showKey(key) {
  if (key instanceof Array) {
    return key.map(x => showKey(x)).toString();
  }
  if (key instanceof ArrayBuffer) {
    return "ArrayBuffer([" + new Uint8Array(key).toString() + "])";
  }
  return key.toString();
}

function* testSteps() {
  const dbname = this.window ? window.location.pathname : "Splendid Test";

  let openRequest = indexedDB.open(dbname, 1);
  openRequest.onerror = errorHandler;
  openRequest.onupgradeneeded = grabEventAndContinueHandler;
  openRequest.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;
  let db = event.target.result;

  // Create test stores
  let store = db.createObjectStore("store");
  let enc = new TextEncoder();

  // Test simple inserts
  // Note: the keys must be in order
  var keys = [
    -1 / 0,
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
    1 / 0,
    new Date("1750-01-02"),
    new Date("1800-12-31T12:34:56.001"),
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
    new Date("3333-03-19T03:33:33.333"),
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
    // Note: enc.encode returns an Uint8Array, which is a valid key, but when
    // converting it back and forth, the result will be a plain ArrayBuffer,
    // which is expected in comparisons below
    // TODO is it ok that the information that the original key was an
    // Uint8Array is lost?
    new ArrayBuffer(0),
    Uint8Array.from([0]).buffer,
    Uint8Array.from([0, 0]).buffer,
    Uint8Array.from([0, 1]).buffer,
    Uint8Array.from([0, 1, 0]).buffer,
    enc.encode("abc").buffer,
    enc.encode("abcd").buffer,
    enc.encode("xyz").buffer,
    Uint8Array.from([0x80]).buffer,
    [],
    [-1 / 0],
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
    // see comment on scalar ArrayBuffers above
    [new ArrayBuffer(0)],
    [new ArrayBuffer(0), "abc"],
    [new ArrayBuffer(0), new ArrayBuffer(0)],
    [new ArrayBuffer(0), enc.encode("abc").buffer],
    [enc.encode("abc").buffer],
    [enc.encode("abc").buffer, new ArrayBuffer(0)],
    [enc.encode("abc").buffer, enc.encode("xyz").buffer],
    [enc.encode("xyz").buffer],
    [[]],
    [[], "foo"],
    [[], []],
    [[[]]],
    [[[]], []],
    [[[]], [[]]],
    [[[]], [[1]]],
    [[[]], [[[]]]],
    [[[1]]],
    [[[[]], []]],
  ];

  for (var i = 0; i < keys.length; ++i) {
    let keyI = keys[i];
    is(indexedDB.cmp(keyI, keyI), 0, i + " compared to self");

    function doCompare(keyI) {
      for (var j = i - 1; j >= i - 10 && j >= 0; --j) {
        is(indexedDB.cmp(keyI, keys[j]), 1, i + " compared to " + j);
        is(indexedDB.cmp(keys[j], keyI), -1, j + " compared to " + i);
      }
    }

    doCompare(keyI);
    store.add(i, keyI).onsuccess = function (e) {
      is(
        indexedDB.cmp(e.target.result, keyI),
        0,
        "Returned key should cmp as equal; index = " +
          i +
          ", input = " +
          showKey(keyI) +
          ", returned = " +
          showKey(e.target.result)
      );
      ok(
        compareKeys(e.target.result, keyI),
        "Returned key should actually be equal; index = " +
          i +
          ", input = " +
          showKey(keyI) +
          ", returned = " +
          showKey(e.target.result)
      );
    };

    // Test that -0 compares the same as 0
    if (keyI === 0) {
      doCompare(-0);
      let req = store.add(i, -0);
      req.addEventListener("error", new ExpectError("ConstraintError", true));
      req.onsuccess = unexpectedSuccessHandler;
      yield undefined;
    } else if (Array.isArray(keyI) && keyI.length === 1 && keyI[0] === 0) {
      doCompare([-0]);
      let req = store.add(i, [-0]);
      req.addEventListener("error", new ExpectError("ConstraintError", true));
      req.onsuccess = unexpectedSuccessHandler;
      yield undefined;
    }
  }

  store.openCursor().onsuccess = grabEventAndContinueHandler;
  for (i = 0; i < keys.length; ++i) {
    event = yield undefined;
    let cursor = event.target.result;
    is(
      indexedDB.cmp(cursor.key, keys[i]),
      0,
      "Read back key should cmp as equal; index = " +
        i +
        ", input = " +
        showKey(keys[i]) +
        ", readBack = " +
        showKey(cursor.key)
    );
    ok(
      compareKeys(cursor.key, keys[i]),
      "Read back key should actually be equal; index = " +
        i +
        ", input = " +
        showKey(keys[i]) +
        ", readBack = " +
        showKey(cursor.key)
    );
    is(cursor.value, i, "Stored with right value");

    cursor.continue();
  }
  event = yield undefined;
  is(event.target.result, null, "no more results expected");

  // Note that nan is defined below as '0 / 0'.
  var invalidKeys = [
    "nan",
    "undefined",
    "null",
    "/x/",
    "{}",
    "new Date(NaN)",
    'new Date("foopy")',
    "[nan]",
    "[undefined]",
    "[null]",
    "[/x/]",
    "[{}]",
    "[new Date(NaN)]",
    "[1, nan]",
    "[1, undefined]",
    "[1, null]",
    "[1, /x/]",
    "[1, {}]",
    "[1, [nan]]",
    "[1, [undefined]]",
    "[1, [null]]",
    "[1, [/x/]]",
    "[1, [{}]]",
    // ATTENTION, the following key allocates 2GB of memory and might cause
    // subtle failures in some environments, see bug 1796753. We might
    // want to have some common way between IndexeDB mochitests and
    // xpcshell tests how to access AppConstants in order to dynamically
    // exclude this key from some environments, rather than disabling the
    // entire xpcshell variant of this test for ASAN/TSAN.
    "new Uint8Array(2147483647)",
  ];

  function checkInvalidKeyException(ex, i, callText) {
    let suffix = ` during ${callText} with invalid key ${i}: ${invalidKeys[i]}`;
    // isInstance() is not available in mochitest, and we use this JS also as mochitest.
    // eslint-disable-next-line mozilla/use-isInstance
    ok(ex instanceof DOMException, "Threw DOMException" + suffix);
    is(ex.name, "DataError", "Threw right DOMException" + suffix);
    is(ex.code, 0, "Threw with right code" + suffix);
  }

  for (i = 0; i < invalidKeys.length; ++i) {
    let key_fn = Function(
      `"use strict"; var nan = 0 / 0; let k = (${invalidKeys[i]}); return k;`
    );
    let key;
    try {
      key = key_fn();
    } catch (e) {
      // If we cannot instantiate the key, we are most likely on a 32 Bit
      // platform with insufficient memory. Just skip it.
      info("Key instantiation failed, skipping");
      continue;
    }
    try {
      indexedDB.cmp(key, 1);
      ok(false, "didn't throw");
    } catch (ex) {
      checkInvalidKeyException(ex, i, "cmp(key, 1)");
    }
    try {
      indexedDB.cmp(1, key);
      ok(false, "didn't throw2");
    } catch (ex) {
      checkInvalidKeyException(ex, i, "cmp(1, key)");
    }
    try {
      store.put(1, key);
      ok(false, "didn't throw3");
    } catch (ex) {
      checkInvalidKeyException(ex, i, "store.put(1, key)");
    }
  }

  openRequest.onsuccess = grabEventAndContinueHandler;
  yield undefined;

  finishTest();
}
