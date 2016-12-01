/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name =
    this.window ? window.location.pathname : "test_wasm_put_get_values.js";

  const objectStoreName = "Wasm";

  const wasmData = { key: 1, wasm: null };

  if (!isWasmSupported()) {
    finishTest();
    return;
  }

  getWasmBinary('(module (func (nop)))');
  let binary = yield undefined;

  wasmData.wasm = getWasmModule(binary);

  info("Opening database");

  let request = indexedDB.open(name);
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

  info("Storing wasm");

  let objectStore = db.transaction([objectStoreName], "readwrite")
                      .objectStore(objectStoreName);
  request = objectStore.add(wasmData.wasm, wasmData.key);
  request.onsuccess = continueToNextStepSync;
  yield undefined;

  is(request.result, wasmData.key, "Got correct key");

  info("Getting wasm");

  request = objectStore.get(wasmData.key);
  request.onsuccess = continueToNextStepSync;
  yield undefined;

  verifyWasmModule(request.result, wasmData.wasm);
  yield undefined;

  info("Getting wasm in new transaction");

  request = db.transaction([objectStoreName])
              .objectStore(objectStoreName).get(wasmData.key);
  request.onsuccess = continueToNextStepSync;
  yield undefined;

  verifyWasmModule(request.result, wasmData.wasm);
  yield undefined;

  finishTest();
}
