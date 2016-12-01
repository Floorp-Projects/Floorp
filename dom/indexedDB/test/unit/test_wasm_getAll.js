/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name =
    this.window ? window.location.pathname : "test_wasm_getAll.js";

  const objectStoreInfo = { name: "Wasm", options: { autoIncrement: true } };

  const values = [ 42, [], [] ];

  if (!isWasmSupported()) {
    finishTest();
    return;
  }

  getWasmBinary('(module (func (result i32) (i32.const 1)))');
  let binary = yield undefined;
  values[1].push(getWasmModule(binary));

  getWasmBinary('(module (func (result i32) (i32.const 2)))');
  binary = yield undefined;
  values[1].push(getWasmModule(binary));

  getWasmBinary('(module (func (result i32) (i32.const 3)))');
  binary = yield undefined;
  values[1].push(getWasmModule(binary));

  getWasmBinary('(module (func (result i32) (i32.const 4)))');
  binary = yield undefined;
  values[2].push(getWasmModule(binary));

  getWasmBinary('(module (func (result i32) (i32.const 5)))');
  binary = yield undefined;
  values[2].push(getWasmModule(binary));

  getWasmBinary('(module (func (result i32) (i32.const 6)))');
  binary = yield undefined;
  values[2].push(getWasmModule(binary));

  getWasmBinary('(module (func (result i32) (i32.const 7)))');
  binary = yield undefined;
  values[2].push(getWasmModule(binary));

  getWasmBinary('(module (func (result i32) (i32.const 8)))');
  binary = yield undefined;
  values[2].push(getWasmModule(binary));

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = continueToNextStepSync;
  request.onsuccess = unexpectedSuccessHandler;
  yield undefined;

  // upgradeneeded
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = continueToNextStepSync;

  info("Creating objectStore");

  request.result.createObjectStore(objectStoreInfo.name,
                                   objectStoreInfo.options);

  yield undefined;

  // success
  let db = request.result;
  db.onerror = errorHandler;

  info("Storing values");

  let objectStore = db.transaction([objectStoreInfo.name], "readwrite")
                      .objectStore(objectStoreInfo.name);
  let addedCount = 0;
  for (let i in values) {
    request = objectStore.add(values[i]);
    request.onsuccess = function(event) {
      if (++addedCount == values.length) {
        continueToNextStep();
      }
    }
  }
  yield undefined;

  info("Getting values");

  request = db.transaction(objectStoreInfo.name)
              .objectStore(objectStoreInfo.name).getAll();
  request.onsuccess = continueToNextStepSync;
  yield undefined;

  // Can't call yield inside of the verify function.
  let resultsToProcess  = [];

  function verifyResult(result, value) {
    if (value instanceof WebAssembly.Module) {
      resultsToProcess.push({ result: result, value: value });
    } else if (value instanceof Array) {
      is(result instanceof Array, true, "Got an array object");
      is(result.length, value.length, "Same length");
      for (let i in value) {
        verifyResult(result[i], value[i]);
      }
    } else {
      is(result, value, "Same value");
    }
  }

  verifyResult(request.result, values);

  for (let resultToProcess of resultsToProcess) {
    verifyWasmModule(resultToProcess.result, resultToProcess.value);
    yield undefined;
  }

  finishTest();
}
