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
    { key: 1, value: 42 },
    { key: 2, value: [null, null, null] },
    { key: 3, value: [null, null, null, null, null] }
  ];

  if (!isWasmSupported()) {
    finishTest();
    return;
  }

  getWasmBinary("(module (func (result i32) (i32.const 1)))");
  let binary = yield undefined;
  wasmData[1].value[0] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 2)))");
  binary = yield undefined;
  wasmData[1].value[1] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 3)))");
  binary = yield undefined;
  wasmData[1].value[2] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 4)))");
  binary = yield undefined;
  wasmData[2].value[0] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 5)))");
  binary = yield undefined;
  wasmData[2].value[1] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 6)))");
  binary = yield undefined;
  wasmData[2].value[2] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 7)))");
  binary = yield undefined;
  wasmData[2].value[3] = getWasmModule(binary);

  getWasmBinary("(module (func (result i32) (i32.const 8)))");
  binary = yield undefined;
  wasmData[2].value[4] = getWasmModule(binary);

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = continueToNextStepSync;
  request.onsuccess = unexpectedSuccessHandler;
  yield undefined;

  // upgradeneeded
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = continueToNextStepSync;

  info("Creating objectStore");

  request.result.createObjectStore(objectStoreName);

  yield undefined;

  // success
  let db = request.result;
  db.onerror = errorHandler;

  info("Storing values");

  let objectStore = db.transaction([objectStoreName], "readwrite")
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
              .getAll();
  request.onsuccess = continueToNextStepSync;
  yield undefined;

  info("Verifying values");

  // Can't call yield inside of the verify function.
  let modulesToProcess = [];

  function verifyArray(array1, array2) {
    is(array1 instanceof Array, true, "Got an array object");
    is(array1.length, array2.length, "Same length");
  }

  function verifyData(data1, data2) {
    if (data2 instanceof Array) {
      verifyArray(data1, data2);
      for (let i in data2) {
        verifyData(data1[i], data2[i]);
      }
    } else if (data2 instanceof WebAssembly.Module) {
      modulesToProcess.push({ module1: data1, module2: data2 });
    } else {
      is(data1, data2, "Same value");
    }
  }

  verifyArray(request.result, wasmData);
  for (let i in wasmData) {
    verifyData(request.result[i], wasmData[i].value);
  }

  for (let moduleToProcess of modulesToProcess) {
    verifyWasmModule(moduleToProcess.module1, moduleToProcess.module2);
    yield undefined;
  }

  finishTest();
}
