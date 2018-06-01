/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// HeapSnapshot.prototype.takeCensus: by: allocationStack breakdown
//
// Ported from js/src/jit-test/tests/debug/Memory-takeCensus-09.js

function run_test() {
  const g = newGlobal();
  const dbg = new Debugger(g);

  g.eval(`                                              // 1
         var log = [];                                  // 2
         function f() { log.push(allocationMarker()); } // 3
         function g() { f(); }                          // 4
         function h() { f(); }                          // 5
         `);

  // Create one allocationMarker with tracking turned off,
  // so it will have no associated stack.
  g.f();

  dbg.memory.allocationSamplingProbability = 1;

  for (const [func, n] of [[g.f, 20], [g.g, 10], [g.h, 5]]) {
    for (let i = 0; i < n; i++) {
      dbg.memory.trackingAllocationSites = true;
      // All allocations of allocationMarker occur with this line as the oldest
      // stack frame.
      func();
      dbg.memory.trackingAllocationSites = false;
    }
  }

  const census = saveHeapSnapshotAndTakeCensus(
    dbg, { breakdown: {
      by: "objectClass",
      then: {
        by: "allocationStack",
        then: { by: "count", label: "haz stack"},
        noStack: {
          by: "count",
          label: "no haz stack"
        }
      }
    }
    });

  const map = census.AllocationMarker;
  ok(map instanceof Map, "Should be a Map instance");
  equal(map.size, 4, "Should have 4 allocation stacks (including the lack of a stack)");

  // Gather the stacks we are expecting to appear as keys, and
  // check that there are no unexpected keys.
  const stacks = { };

  map.forEach((v, k) => {
    if (k === "noStack") {
      // No need to save this key.
    } else if (k.functionDisplayName === "f" &&
               k.parent.functionDisplayName === "run_test") {
      stacks.f = k;
    } else if (k.functionDisplayName === "f" &&
               k.parent.functionDisplayName === "g" &&
               k.parent.parent.functionDisplayName === "run_test") {
      stacks.fg = k;
    } else if (k.functionDisplayName === "f" &&
               k.parent.functionDisplayName === "h" &&
               k.parent.parent.functionDisplayName === "run_test") {
      stacks.fh = k;
    } else {
      dumpn("Unexpected allocation stack:");
      k.toString().split(/\n/g).forEach(s => dumpn(s));
      ok(false);
    }
  });

  equal(map.get("noStack").label, "no haz stack");
  equal(map.get("noStack").count, 1);

  ok(stacks.f);
  equal(map.get(stacks.f).label, "haz stack");
  equal(map.get(stacks.f).count, 20);

  ok(stacks.fg);
  equal(map.get(stacks.fg).label, "haz stack");
  equal(map.get(stacks.fg).count, 10);

  ok(stacks.fh);
  equal(map.get(stacks.fh).label, "haz stack");
  equal(map.get(stacks.fh).count, 5);

  do_test_finished();
}
