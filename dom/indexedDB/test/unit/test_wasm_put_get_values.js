/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps() {
  const name = this.window
    ? window.location.pathname
    : "test_wasm_put_get_values.js";

  const objectStoreName = "Wasm";

  const wasmData = { key: 1, value: null };

  if (!isWasmSupported()) {
    finishTest();
    return;
  }

  // js -e 'print(wasmTextToBinary(`(module (func (export "run") (result i32) (i32.const 13)))`))'
  // eslint-disable-next-line
  let binary = new Uint8Array([0,97,115,109,1,0,0,0,1,5,1,96,0,1,127,3,2,1,0,7,7,1,3,114,117,110,0,0,10,6,1,4,0,65,13,11]);

  wasmData.value = getWasmModule(binary);

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

  info("Testing failure to store wasm");

  let objectStore = db
    .transaction([objectStoreName], "readwrite")
    .objectStore(objectStoreName);

  // storing a wasm module in IDB should now fail
  let failed = false;
  try {
    objectStore.add(wasmData.value, wasmData.key);
  } catch (err) {
    failed = true;
    ok(err instanceof DOMException, "caught right error type");
    is(err.name, "DataCloneError", "caught right error name");
  }
  ok(failed, "error was thrown");

  finishTest();
}
