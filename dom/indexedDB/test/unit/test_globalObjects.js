/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = "Splendid Test";

  let ioService =
    Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

  function getSpec(filename) {
    let file = do_get_file(filename);
    let uri = ioService.newFileURI(file);
    return uri.spec;
  }

  // Test for IDBKeyRange and indexedDB availability in xpcshell.
  let keyRange = IDBKeyRange.only(42);
  ok(keyRange, "Got keyRange");

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  ok(db, "Got database");

  // Test for IDBKeyRange and indexedDB availability in JS modules.
  Cu.import(getSpec("GlobalObjectsModule.jsm"));
  let test = new GlobalObjectsModule();
  test.ok = ok;
  test.finishTest = continueToNextStep;
  test.runTest();
  yield undefined;

  // Test for IDBKeyRange and indexedDB availability in JS components.
  do_load_manifest("GlobalObjectsComponent.manifest");
  test = Cc["@mozilla.org/dom/indexeddb/GlobalObjectsComponent;1"].
         createInstance(Ci.nsISupports).wrappedJSObject;
  test.ok = ok;
  test.finishTest = continueToNextStep;
  test.runTest();
  yield undefined;

  // Test for IDBKeyRange and indexedDB availability in JS sandboxes.
  let principal = Cc["@mozilla.org/systemprincipal;1"].
                  createInstance(Ci.nsIPrincipal);
  let sandbox = new Cu.Sandbox(principal,
                               { wantGlobalProperties: ["indexedDB"] });
  sandbox.__SCRIPT_URI_SPEC__ = getSpec("GlobalObjectsSandbox.js");
  Cu.evalInSandbox(
    "Components.classes['@mozilla.org/moz/jssubscript-loader;1'] \
               .createInstance(Components.interfaces.mozIJSSubScriptLoader) \
               .loadSubScript(__SCRIPT_URI_SPEC__);", sandbox, "1.7");
  sandbox.ok = ok;
  sandbox.finishTest = continueToNextStep;
  Cu.evalInSandbox("runTest();", sandbox);
  yield undefined;

  finishTest();
  yield undefined;
}
