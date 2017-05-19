/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name =
    this.window ? window.location.pathname : "test_wasm_indexes.js";

  const objectStoreName = "Wasm";

  const wasmData = { key: 1, value: { name: "foo", data: null } };

  const indexData = { name: "nameIndex", keyPath: "name", options: { } };

  if (!isWasmSupported()) {
    finishTest();
    return;
  }

  getWasmBinary("(module (func (nop)))");
  let binary = yield undefined;
  wasmData.value.data = getWasmModule(binary);

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

  let objectStore = request.result.createObjectStore(objectStoreName);

  info("Creating index");

  objectStore.createIndex(indexData.name, indexData.keyPath, indexData.options);

  yield undefined;

  // success
  let db = request.result;
  db.onerror = errorHandler;

  info("Storing wasm");

  objectStore = db.transaction([objectStoreName], "readwrite")
                  .objectStore(objectStoreName);
  request = objectStore.add(wasmData.value, wasmData.key);
  request.onsuccess = continueToNextStepSync;
  yield undefined;

  is(request.result, wasmData.key, "Got correct key");

  info("Getting wasm");

  request = objectStore.index("nameIndex").get("foo");
  request.addEventListener("error", new ExpectError("UnknownError", true));
  request.onsuccess = unexpectedSuccessHandler;
  yield undefined;

  info("Opening cursor");

  request = objectStore.index("nameIndex").openCursor();
  request.addEventListener("error", new ExpectError("UnknownError", true));
  request.onsuccess = unexpectedSuccessHandler;
  yield undefined;

  finishTest();
}
