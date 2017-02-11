/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = "test_wasm_recompile.js";

  const objectStoreName = "Wasm";

  const wasmData = { key: 1, wasm: null };

  // The goal of this test is to prove that wasm is recompiled and the on-disk
  // copy updated.

  if (!isWasmSupported()) {
    finishTest();
    yield undefined;
  }

  getWasmBinary('(module (func (nop)))');
  let binary = yield undefined;

  wasmData.wasm = getWasmModule(binary);

  info("Installing profile");

  clearAllDatabases(continueToNextStepSync);
  yield undefined;

  // The profile was created by an older build (buildId: 20161116145318,
  // cpuId: X64=0x2). It contains one stored wasm module (file id 1 - bytecode
  // and file id 2 - compiled/machine code). The file create_db.js in the
  // package was run locally (specifically it was temporarily added to
  // xpcshell-parent-process.ini and then executed:
  // mach xpcshell-test dom/indexedDB/test/unit/create_db.js
  installPackagedProfile("wasm_recompile_profile");

  let filesDir = getChromeFilesDir();

  let file = filesDir.clone();
  file.append("2");

  info("Reading out contents of compiled blob");

  let fileReader = new FileReader();
  fileReader.onload = continueToNextStepSync;

  let domFile;
  File.createFromNsIFile(file).then(file => { domFile = file; }).then(continueToNextStepSync);
  yield undefined;

  fileReader.readAsArrayBuffer(domFile);

  yield undefined;

  let compiledBuffer = fileReader.result;

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

  request = db.transaction([objectStoreName])
              .objectStore(objectStoreName).get(wasmData.key);
  request.onsuccess = continueToNextStepSync;
  yield undefined;

  info("Verifying wasm module");

  verifyWasmModule(request.result, wasmData.wasm);
  yield undefined;

  info("Reading out contents of new compiled blob");

  fileReader = new FileReader();
  fileReader.onload = continueToNextStepSync;

  File.createFromNsIFile(file).then(file => { domFile = file; }).then(continueToNextStepSync);
  yield undefined;

  fileReader.readAsArrayBuffer(domFile);

  yield undefined;

  let newCompiledBuffer = fileReader.result;

  info("Verifying blobs differ");

  ok(!compareBuffers(newCompiledBuffer, compiledBuffer), "Blobs differ");

  info("Getting wasm again");

  request = db.transaction([objectStoreName])
              .objectStore(objectStoreName).get(wasmData.key);
  request.onsuccess = continueToNextStepSync;
  yield undefined;

  info("Verifying wasm module");

  verifyWasmModule(request.result, wasmData.wasm);
  yield undefined;

  info("Reading contents of new compiled blob again");

  fileReader = new FileReader();
  fileReader.onload = continueToNextStepSync;

  File.createFromNsIFile(file).then(file => { domFile = file; }).then(continueToNextStepSync);
  yield undefined;

  fileReader.readAsArrayBuffer(domFile);

  yield undefined;

  let newCompiledBuffer2 = fileReader.result;

  info("Verifying blob didn't change");

  ok(compareBuffers(newCompiledBuffer2, newCompiledBuffer),
     "Blob didn't change");

  finishTest();
  yield undefined;
}
