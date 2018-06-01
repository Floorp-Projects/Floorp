/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that HeapSnapshot.prototype.takeCensus finds GC roots that are on the
// stack.
//
// Ported from js/src/jit-test/tests/debug/Memory-takeCensus-04.js

function run_test() {
  const g = newGlobal();
  const dbg = new Debugger(g);

  g.eval(`
function withAllocationMarkerOnStack(f) {
  (function () {
    var onStack = allocationMarker();
    f();
  }());
}
`);

  equal("AllocationMarker" in saveHeapSnapshotAndTakeCensus(dbg).objects, false,
        "There shouldn't exist any allocation markers in the census.");

  let allocationMarkerCount;
  g.withAllocationMarkerOnStack(() => {
    const census = saveHeapSnapshotAndTakeCensus(dbg);
    allocationMarkerCount = census.objects.AllocationMarker.count;
  });

  equal(allocationMarkerCount, 1,
        "Should have one allocation marker in the census, because there " +
        "was one on the stack.");

  do_test_finished();
}
