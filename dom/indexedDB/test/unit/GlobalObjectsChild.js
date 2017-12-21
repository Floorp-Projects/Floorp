/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* import-globals-from xpcshell-head-parent-process.js */

function ok(cond, msg) {
  dump("ok(" + cond + ", \"" + msg + "\")");
  Assert.ok(!!cond, Components.stack.caller);
}

function finishTest()
{
  do_execute_soon(function() {
    do_test_finished();
  });
}

function run_test() {
  const name = "Splendid Test";

  Cu.importGlobalProperties(["indexedDB"]);

  do_test_pending();

  let keyRange = IDBKeyRange.only(42);
  ok(keyRange, "Got keyRange");

  let request = indexedDB.open(name, 1);
  request.onerror = function(event) {
    ok(false, "indexedDB error, '" + event.target.error.name + "'");
    finishTest();
  };
  request.onsuccess = function(event) {
    let db = event.target.result;
    ok(db, "Got database");
    finishTest();
  };
}
