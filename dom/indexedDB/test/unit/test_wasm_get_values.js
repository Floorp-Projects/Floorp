/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps() {
  const name = "test_wasm_recompile.js";

  const objectStoreName = "Wasm";

  const wasmData = { key: 1 };

  // The goal of this test is to prove that stored wasm is never deserialized.

  info("Installing profile");

  clearAllDatabases(continueToNextStepSync);
  yield undefined;

  // The profile was created with a mythical build (buildId: 20180309213541,
  // cpuId: X64=0x2). It contains one stored wasm module (file id 1 - bytecode
  // and file id 2 - compiled/machine code). The file create_db.js in the
  // package was run locally (specifically it was temporarily added to
  // xpcshell-parent-process.ini and then executed:
  // mach xpcshell-test dom/indexedDB/test/unit/create_db.js
  installPackagedProfile("wasm_get_values_profile");

  info("Opening database");

  let request = indexedDB.open(name);
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = continueToNextStepSync;
  yield undefined;

  // success
  let db = request.result;
  db.onerror = errorHandler;

  info("Getting wasm");

  request = db
    .transaction([objectStoreName])
    .objectStore(objectStoreName)
    .get(wasmData.key);
  request.onsuccess = continueToNextStepSync;
  yield undefined;

  info("Verifying wasm");

  let isWasmModule = request.result instanceof WebAssembly.Module;
  ok(!isWasmModule, "Object is not wasm module");

  finishTest();
  yield undefined;
}
