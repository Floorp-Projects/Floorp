/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// HeapSnapshot.prototype.takeCensus returns a value of an appropriate
// shape. Ported from js/src/jit-tests/debug/Memory-takeCensus-01.js

function run_test() {
  var dbg = new Debugger;

  function checkProperties(census) {
    equal(typeof census, "object");
    for (prop of Object.getOwnPropertyNames(census)) {
      var desc = Object.getOwnPropertyDescriptor(census, prop);
      equal(desc.enumerable, true);
      equal(desc.configurable, true);
      equal(desc.writable, true);
      if (typeof desc.value === "object")
        checkProperties(desc.value);
      else
        equal(typeof desc.value, "number");
    }
  }

  checkProperties(saveHeapSnapshotAndTakeCensus(dbg));

  var g = newGlobal();
  dbg.addDebuggee(g);
  checkProperties(saveHeapSnapshotAndTakeCensus(dbg));

  do_test_finished();
}
