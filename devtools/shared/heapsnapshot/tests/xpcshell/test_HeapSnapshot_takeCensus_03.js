/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// HeapSnapshot.prototype.takeCensus behaves plausibly as we add and remove
// debuggees.
//
// Ported from js/src/jit-test/tests/debug/Memory-takeCensus-03.js

function run_test() {
  const dbg = new Debugger();

  const census0 = saveHeapSnapshotAndTakeCensus(dbg);
  Census.walkCensus(census0, "census0", Census.assertAllZeros);

  const g1 = newGlobal();
  dbg.addDebuggee(g1);
  const census1 = saveHeapSnapshotAndTakeCensus(dbg);
  Census.walkCensus(census1, "census1", Census.assertAllNotLessThan(census0));

  const g2 = newGlobal();
  dbg.addDebuggee(g2);
  const census2 = saveHeapSnapshotAndTakeCensus(dbg);
  Census.walkCensus(census2, "census2", Census.assertAllNotLessThan(census1));

  dbg.removeDebuggee(g2);
  const census3 = saveHeapSnapshotAndTakeCensus(dbg);
  Census.walkCensus(census3, "census3", Census.assertAllEqual(census1));

  dbg.removeDebuggee(g1);
  const census4 = saveHeapSnapshotAndTakeCensus(dbg);
  Census.walkCensus(census4, "census4", Census.assertAllEqual(census0));

  do_test_finished();
}
