/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Check byte counts produced by takeCensus.
//
// Ported from js/src/jit-test/tests/debug/Memory-take Census-10.js

function run_test() {
  let g = newGlobal();
  let dbg = new Debugger(g);

  let sizeOfAM = byteSize(allocationMarker());

  // Allocate a single allocation marker, and check that we can find it.
  g.eval('var hold = allocationMarker();');
  let census = saveHeapSnapshotAndTakeCensus(dbg, { breakdown: { by: 'objectClass' } });
  equal(census.AllocationMarker.count, 1);
  equal(census.AllocationMarker.bytes, sizeOfAM);
  g.hold = null;

  g.eval(`                                  // 1
         var objs = [];                     // 2
         function fnerd() {                 // 3
           objs.push(allocationMarker());   // 4
           for (let i = 0; i < 10; i++)     // 5
             objs.push(allocationMarker()); // 6
         }                                  // 7
         `);                                // 8

  dbg.memory.allocationSamplingProbability = 1;
  dbg.memory.trackingAllocationSites = true;
  g.fnerd();
  dbg.memory.trackingAllocationSites = false;

  census = saveHeapSnapshotAndTakeCensus(dbg, {
    breakdown: { by: 'objectClass',
                 then: { by: 'allocationStack' }
               }
  });

  let seen = 0;
  census.AllocationMarker.forEach((v, k) => {
    equal(k.functionDisplayName, 'fnerd');
    switch (k.line) {
    case 4:
      equal(v.count, 1);
      equal(v.bytes, sizeOfAM);
      seen++;
      break;

    case 6:
      equal(v.count, 10);
      equal(v.bytes, 10 * sizeOfAM);
      seen++;
      break;

    default:
      dumpn("Unexpected stack:");
      k.toString().split(/\n/g).forEach(s => dumpn(s));
      ok(false);
      break;
    }
  });

  equal(seen, 2);

  do_test_finished();
}
