/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that HeapSnapshot.prototype.takeCensus finds cross compartment
// wrapper GC roots.
//
// Ported from js/src/jit-test/tests/debug/Memory-takeCensus-05.js

/* eslint-disable strict */
function run_test() {
  const g = newGlobal();
  const dbg = new Debugger(g);

  equal("AllocationMarker" in saveHeapSnapshotAndTakeCensus(dbg).objects, false,
        "No allocation markers should exist in the census.");

  this.ccw = g.allocationMarker();

  const census = saveHeapSnapshotAndTakeCensus(dbg);
  equal(census.objects.AllocationMarker.count, 1,
        "Should have one allocation marker in the census, because there " +
        "is one cross-compartment wrapper referring to it.");

  do_test_finished();
}
