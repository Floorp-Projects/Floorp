/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name =
    this.window ? window.location.pathname : "test_wasm_getAll.js";

  const objectStoreName = "Wasm";

  const wasmData = [
    { key: 1, value: { name: "foo1", data: 42 } },
    { key: 2, value: { name: "foo2", data: [null, null, null] } },
    { key: 3, value: { name: "foo3", data: [null, null, null, null, null] } }
  ];

  const indexData = { name: "nameIndex", keyPath: "name", options: { } };

  if (!isWasmSupported()) {
    finishTest();
    return;
  }

  getWasmBinary("(module (func (result i32) (i32.const 1)))");
  let binary = yield undefined;
  wasmData[1].value.data[0] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 2)))");
  binary = yield undefined;
  wasmData[1].value.data[1] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 3)))");
  binary = yield undefined;
  wasmData[1].value.data[2] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 4)))");
  binary = yield undefined;
  wasmData[2].value.data[0] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 5)))");
  binary = yield undefined;
  wasmData[2].value.data[1] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 6)))");
  binary = yield undefined;
  wasmData[2].value.data[2] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 7)))");
  binary = yield undefined;
  wasmData[2].value.data[3] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 8)))");
  binary = yield undefined;
  wasmData[2].value.data[4] = getWasmModule(binary);

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = continueToNextStepSync;
  request.onsuccess = unexpectedSuccessHandler;
  yield undefined;

  // upgradeneeded
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = continueToNextStepSync;

  info("Creating objectStore");

  let objectStore = request.result.createObjectStore(objectStoreName);

  info("Creating index");

  objectStore.createIndex(indexData.name, indexData.keyPath, indexData.options);

  yield undefined;

  // success
  let db = request.result;
  db.onerror = errorHandler;

  info("Storing values");

  objectStore = db.transaction([objectStoreName], "readwrite")
                  .objectStore(objectStoreName);
  let addedCount = 0;
  for (let i in wasmData) {
    request = objectStore.add(wasmData[i].value, wasmData[i].key);
    request.onsuccess = function(event) {
      if (++addedCount == wasmData.length) {
        continueToNextStep();
      }
    };
  }
  yield undefined;

  info("Getting values");

  request = db.transaction(objectStoreName)
              .objectStore(objectStoreName)
              .index("nameIndex")
              .getAll();
  request.addEventListener("error", new ExpectError("UnknownError", true));
  request.onsuccess = unexpectedSuccessHandler;
  yield undefined;

  finishTest();
}
