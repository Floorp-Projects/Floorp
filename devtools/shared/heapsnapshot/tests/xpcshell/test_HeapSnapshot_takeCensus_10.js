/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Check byte counts produced by takeCensus.
//
// Note that tracking allocation sites adds unique IDs to objects which
// increases their size.
//
// Ported from js/src/jit-test/tests/debug/Memory-take Census-10.js

function run_test() {
  const g = newGlobal();
  const dbg = new Debugger(g);

  // Allocate a single allocation marker, and check that we can find it.
  dbg.memory.trackingAllocationSites = true;
  g.eval("var hold = allocationMarker();");
  dbg.memory.trackingAllocationSites = false;
  let census = dbg.memory.takeCensus({
    breakdown: { by: "objectClass", then: { by: "allocationStack" } },
  });
  let markers = 0;
  let count;
  let sizeOfAM;
  census.AllocationMarker.forEach((v, k) => {
    count = v.count;
    sizeOfAM = v.bytes;
    markers++;
  });
  equal(markers, 1);
  equal(count, 1);
  g.hold = null;

  g.eval(`                                  // 1
         var objs = [];                     // 2
         function fnerd() {                 // 3
           objs.push(allocationMarker());   // 4
           for (let i = 0; i < 10; i++)     // 5
             objs.push(allocationMarker()); // 6
         }                                  // 7
         `);

  dbg.memory.allocationSamplingProbability = 1;
  dbg.memory.trackingAllocationSites = true;
  g.fnerd();
  dbg.memory.trackingAllocationSites = false;

  census = saveHeapSnapshotAndTakeCensus(dbg, {
    breakdown: { by: "objectClass", then: { by: "allocationStack" } },
  });

  let seen = 0;
  census.AllocationMarker.forEach((v, k) => {
    equal(k.functionDisplayName, "fnerd");
    switch (k.line) {
      case 4:
        equal(v.count, 1);
        equal(v.bytes, sizeOfAM);
        seen++;
        break;

      case 6:
        equal(v.count, 10);
        equal(v.bytes >= 10 * sizeOfAM, true);
        seen++;
        break;

      default:
        dumpn("Unexpected stack:");
        k.toString()
          .split(/\n/g)
          .forEach(s => dumpn(s));
        ok(false);
        break;
    }
  });

  equal(seen, 2);

  do_test_finished();
}
