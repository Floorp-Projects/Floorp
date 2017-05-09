/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  // Test object stores

  const name = "test_complex_keyPaths";
  const keyPaths = [
    { keyPath: "id",      value: { id: 5 },                      key: 5 },
    { keyPath: "id",      value: { id: "14", iid: 12 },          key: "14" },
    { keyPath: "id",      value: { iid: "14", id: 12 },          key: 12 },
    { keyPath: "id",      value: {} },
    { keyPath: "id",      value: { id: {} } },
    { keyPath: "id",      value: { id: /x/ } },
    { keyPath: "id",      value: 2 },
    { keyPath: "id",      value: undefined },
    { keyPath: "foo.id",  value: { foo: { id: 7 } },             key: 7 },
    { keyPath: "foo.id",  value: { id: 7, foo: { id: "asdf" } }, key: "asdf" },
    { keyPath: "foo.id",  value: { foo: { id: undefined } } },
    { keyPath: "foo.id",  value: { foo: 47 } },
    { keyPath: "foo.id",  value: {} },
    { keyPath: "",        value: "foopy",                        key: "foopy" },
    { keyPath: "",        value: 2,                              key: 2 },
    { keyPath: "",        value: undefined },
    { keyPath: "",        value: { id: 12 } },
    { keyPath: "",        value: /x/ },
    { keyPath: "foo.bar", value: { baz: 1, foo: { baz2: 2, bar: "xo" } },     key: "xo" },
    { keyPath: "foo.bar.baz", value: { foo: { bar: { bazz: 16, baz: 17 } } }, key: 17 },
    { keyPath: "foo..id", exception: true },
    { keyPath: "foo.",    exception: true },
    { keyPath: "fo o",    exception: true },
    { keyPath: "foo ",    exception: true },
    { keyPath: "foo[bar]",exception: true },
    { keyPath: "foo[1]",  exception: true },
    { keyPath: "$('id').stuff", exception: true },
    { keyPath: "foo.2.bar", exception: true },
    { keyPath: "foo. .bar", exception: true },
    { keyPath: ".bar",    exception: true },
    { keyPath: [],        exception: true },

    { keyPath: ["foo", "bar"],        value: { foo: 1, bar: 2 },              key: [1, 2] },
    { keyPath: ["foo"],               value: { foo: 1, bar: 2 },              key: [1] },
    { keyPath: ["foo", "bar", "bar"], value: { foo: 1, bar: "x" },            key: [1, "x", "x"] },
    { keyPath: ["x", "y"],            value: { x: [],  y: "x" },              key: [[], "x"] },
    { keyPath: ["x", "y"],            value: { x: [[1]],  y: "x" },           key: [[[1]], "x"] },
    { keyPath: ["x", "y"],            value: { x: [[1]],  y: new Date(1) },   key: [[[1]], new Date(1)] },
    { keyPath: ["x", "y"],            value: { x: [[1]],  y: [new Date(3)] }, key: [[[1]], [new Date(3)]] },
    { keyPath: ["x", "y.bar"],        value: { x: "hi", y: { bar: "x"} },     key: ["hi", "x"] },
    { keyPath: ["x.y", "y.bar"],      value: { x: { y: "hello" }, y: { bar: "nurse"} }, key: ["hello", "nurse"] },
    { keyPath: ["", ""],              value: 5,                               key: [5, 5] },
    { keyPath: ["x", "y"],            value: { x: 1 } },
    { keyPath: ["x", "y"],            value: { y: 1 } },
    { keyPath: ["x", "y"],            value: { x: 1, y: undefined } },
    { keyPath: ["x", "y"],            value: { x: null, y: 1 } },
    { keyPath: ["x", "y.bar"],        value: { x: null, y: { bar: "x"} } },
    { keyPath: ["x", "y"],            value: { x: 1, y: false } },
    { keyPath: ["x", "y", "z"],       value: { x: 1, y: false, z: "a" } },
    { keyPath: [".x", "y", "z"],      exception: true },
    { keyPath: ["x", "y ", "z"],      exception: true },
  ];

  let openRequest = indexedDB.open(name, 1);
  openRequest.onerror = errorHandler;
  openRequest.onupgradeneeded = grabEventAndContinueHandler;
  openRequest.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;
  let db = event.target.result;

  let stores = {};

  // Test creating object stores and inserting data
  for (let i = 0; i < keyPaths.length; i++) {
    let info = keyPaths[i];

    let test = " for objectStore test " + JSON.stringify(info);
    let indexName = JSON.stringify(info.keyPath);
    if (!stores[indexName]) {
      try {
        let objectStore = db.createObjectStore(indexName, { keyPath: info.keyPath });
        ok(!("exception" in info), "shouldn't throw" + test);
        is(JSON.stringify(objectStore.keyPath), JSON.stringify(info.keyPath),
           "correct keyPath property" + test);
        ok(objectStore.keyPath === objectStore.keyPath,
           "object identity should be preserved");
        stores[indexName] = objectStore;
      } catch (e) {
        ok("exception" in info, "should throw" + test);
        is(e.name, "SyntaxError", "expect a SyntaxError" + test);
        ok(e instanceof DOMException, "Got a DOM Exception" + test);
        is(e.code, DOMException.SYNTAX_ERR, "expect a syntax error" + test);
        continue;
      }
    }

    let store = stores[indexName];

    try {
      request = store.add(info.value);
      ok("key" in info, "successfully created request to insert value" + test);
    } catch (e) {
      ok(!("key" in info), "threw when attempted to insert" + test);
      ok(e instanceof DOMException, "Got a DOMException" + test);
      is(e.name, "DataError", "expect a DataError" + test);
      is(e.code, 0, "expect zero" + test);
      continue;
    }

    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;

    let e = yield undefined;
    is(e.type, "success", "inserted successfully" + test);
    is(e.target, request, "expected target" + test);
    ok(compareKeys(request.result, info.key), "found correct key" + test);
    is(indexedDB.cmp(request.result, info.key), 0, "returned key compares correctly" + test);

    store.get(info.key).onsuccess = grabEventAndContinueHandler;
    e = yield undefined;
    isnot(e.target.result, undefined, "Did find entry");

    // Check that cursor.update work as expected
    request = store.openCursor();
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    e = yield undefined;
    let cursor = e.target.result;
    request = cursor.update(info.value);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    yield undefined;
    ok(true, "Successfully updated cursor" + test);

    // Check that cursor.update throws as expected when key is changed
    let newValue = cursor.value;
    let destProp = Array.isArray(info.keyPath) ? info.keyPath[0] : info.keyPath;
    if (destProp) {
      // eslint-disable-next-line no-eval
      eval("newValue." + destProp + " = 'newKeyValue'");
    }
    else {
      newValue = 'newKeyValue';
    }
    let didThrow;
    try {
      cursor.update(newValue);
    }
    catch (ex) {
      didThrow = ex;
    }
    ok(didThrow instanceof DOMException, "Got a DOMException" + test);
    is(didThrow.name, "DataError", "expect a DataError" + test);
    is(didThrow.code, 0, "expect zero" + test);

    // Clear object store to prepare for next test
    store.clear().onsuccess = grabEventAndContinueHandler;
    yield undefined;
  }

  // Attempt to create indexes and insert data
  let store = db.createObjectStore("indexStore");
  let indexes = {};
  for (let i = 0; i < keyPaths.length; i++) {
    let info = keyPaths[i];
    let test = " for index test " + JSON.stringify(info);
    let indexName = JSON.stringify(info.keyPath);
    if (!indexes[indexName]) {
      try {
        let index = store.createIndex(indexName, info.keyPath);
        ok(!("exception" in info), "shouldn't throw" + test);
        is(JSON.stringify(index.keyPath), JSON.stringify(info.keyPath),
           "index has correct keyPath property" + test);
        ok(index.keyPath === index.keyPath,
           "object identity should be preserved");
        indexes[indexName] = index;
      } catch (e) {
        ok("exception" in info, "should throw" + test);
        is(e.name, "SyntaxError", "expect a SyntaxError" + test);
        ok(e instanceof DOMException, "Got a DOM Exception" + test);
        is(e.code, DOMException.SYNTAX_ERR, "expect a syntax error" + test);
        continue;
      }
    }
    
    let index = indexes[indexName];

    request = store.add(info.value, 1);
    if ("key" in info) {
      index.getKey(info.key).onsuccess = grabEventAndContinueHandler;
      e = yield undefined;
      is(e.target.result, 1, "found value when reading" + test);
    }
    else {
      index.count().onsuccess = grabEventAndContinueHandler;
      e = yield undefined;
      is(e.target.result, 0, "should be empty" + test);
    }

    store.clear().onsuccess = grabEventAndContinueHandler;
    yield undefined;
  }

  // Autoincrement and complex key paths
  let aitests = [{ v: {},                           k: 1, res: { foo: { id: 1 }} },
                 { v: { value: "x" },               k: 2, res: { value: "x", foo: { id: 2 }} },
                 { v: { value: "x", foo: {} },      k: 3, res: { value: "x", foo: { id: 3 }} },
                 { v: { v: "x", foo: { x: "y" } },  k: 4, res: { v: "x", foo: { x: "y", id: 4 }} },
                 { v: { value: 2, foo: { id: 10 }}, k: 10 },
                 { v: { value: 2 },                 k: 11, res: { value: 2, foo: { id: 11 }} },
                 { v: true,                         },
                 { v: { value: 2, foo: 12 },        },
                 { v: { foo: { id: true }},         },
                 { v: { foo: { x: 5, id: {} }},     },
                 { v: undefined,                    },
                 { v: { foo: undefined },           },
                 { v: { foo: { id: undefined }},    },
                 { v: null,                         },
                 { v: { foo: null },                },
                 { v: { foo: { id: null }},         },
                 ];

  store = db.createObjectStore("gen", { keyPath: "foo.id", autoIncrement: true });
  for (let i = 0; i < aitests.length; ++i) {
    let info = aitests[i];
    let test = " for autoIncrement test " + JSON.stringify(info);

    let preValue = JSON.stringify(info.v);
    if ("k" in info) {
      store.add(info.v).onsuccess = grabEventAndContinueHandler;
      is(JSON.stringify(info.v), preValue, "put didn't modify value" + test);
    }
    else {
      try {
        store.add(info.v);
        ok(false, "should throw" + test);
      }
      catch(e) {
        ok(true, "did throw" + test);
        ok(e instanceof DOMException, "Got a DOMException" + test);
        is(e.name, "DataError", "expect a DataError" + test);
        is(e.code, 0, "expect zero" + test);

        is(JSON.stringify(info.v), preValue, "failing put didn't modify value" + test);

        continue;
      }
    }

    let e = yield undefined;
    is(e.target.result, info.k, "got correct return key" + test);

    store.get(info.k).onsuccess = grabEventAndContinueHandler;
    e = yield undefined;
    is(JSON.stringify(e.target.result), JSON.stringify(info.res || info.v),
       "expected value stored" + test);
  }

  openRequest.onsuccess = grabEventAndContinueHandler;
  yield undefined;

  finishTest();
}
