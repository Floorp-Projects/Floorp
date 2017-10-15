/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name =
    this.window ? window.location.pathname : "test_wasm_serialize_tiering.js";

  const objectStoreName = "Wasm";

  if (!isWasmSupported()) {
    finishTest();
    return;
  }

  // Make a module big enough so that tiering is significant.
  const N = 50;
  let bigFunc = `(func (result f64)\n`;
  for (let i = 0; i < N; i++) {
    bigFunc += `  f64.const 1.0\n`;
  }
  for (let i = 0; i < N - 1; i++) {
    bigFunc += `  f64.add\n`;
  }
  bigFunc += `)`;
  let bigModule = `(module \n`;
  for (let i = 0; i < 100; i++) {
    bigModule += bigFunc;
  }
  bigModule += `  (export "run" (func 10))\n`;
  bigModule += `)`;

  getWasmBinary(bigModule);
  let bigBinary = yield undefined;

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

  info("Storing wasm modules");

  let objectStore = db.transaction([objectStoreName], "readwrite")
                      .objectStore(objectStoreName);

  const NumModules = 5;
  const NumCopies = 5;

  let finishedAdds = 0;
  for (let moduleIndex = 0; moduleIndex < NumModules; moduleIndex++) {
    let module = new WebAssembly.Module(bigBinary);
    for (let copyIndex = 0; copyIndex < NumCopies; copyIndex++) {
      let key = String(moduleIndex) + " " + String(copyIndex);
      let request = objectStore.add(module, key);
      request.onsuccess = function() {
        is(request.result, key, "Got correct key");
        if (++finishedAdds === NumModules * NumCopies) {
          continueToNextStepSync();
        }
      };
    }
  }
  yield undefined;

  info("Getting wasm");

  let finishedGets = 0;
  for (let moduleIndex = 0; moduleIndex < NumModules; moduleIndex++) {
    for (let copyIndex = 0; copyIndex < NumCopies; copyIndex++) {
      let key = String(moduleIndex) + " " + String(copyIndex);
      let request = objectStore.get(key);
      request.onsuccess = function() {
        let module = request.result;
        let instance = new WebAssembly.Instance(module);
        is(instance.exports.run(), N, "Got correct run() result");
        if (++finishedGets === NumModules * NumCopies) {
          continueToNextStepSync();
        }
      };
    }
  }
  yield undefined;

  finishTest();
}
