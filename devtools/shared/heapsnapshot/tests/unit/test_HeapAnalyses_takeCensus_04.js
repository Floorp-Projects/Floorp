/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the HeapAnalyses{Client,Worker} can send SavedFrame stacks from
// by-allocation-stack reports from the worker.

add_task(async function test() {
  const client = new HeapAnalysesClient();

  // Track some allocation stacks.

  const g = newGlobal();
  const dbg = new Debugger(g);
  g.eval(`                                                   // 1
         this.log = [];                                      // 2
         function f() { this.log.push(allocationMarker()); } // 3
         function g() { this.log.push(allocationMarker()); } // 4
         function h() { this.log.push(allocationMarker()); } // 5
         `);

  // Create one allocationMarker with tracking turned off,
  // so it will have no associated stack.
  g.f();

  dbg.memory.allocationSamplingProbability = 1;

  for (let [func, n] of [ [g.f, 20],
                          [g.g, 10],
                          [g.h, 5] ]) {
    for (let i = 0; i < n; i++) {
      dbg.memory.trackingAllocationSites = true;
      // All allocations of allocationMarker occur with this line as the oldest
      // stack frame.
      func();
      dbg.memory.trackingAllocationSites = false;
    }
  }

  // Take a heap snapshot.

  const snapshotFilePath = saveNewHeapSnapshot({ debugger: dbg });
  await client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  // Run a census broken down by class name -> allocation stack so we can grab
  // only the AllocationMarker objects we have complete control over.

  const { report } = await client.takeCensus(
    snapshotFilePath,
    {
      breakdown: {
        by: "objectClass",
        then: {
          by: "allocationStack",
          then: {
            by: "count",
            bytes: true,
            count: true
          },
          noStack: {
            by: "count",
            bytes: true,
            count: true
          }
        }
      }
    });

  // Test the generated report.

  ok(report, "Should get a report");

  const map = report.AllocationMarker;
  ok(map, "Should get AllocationMarkers in the report.");
  // From a module with a different global, and therefore a different Map
  // constructor, so we can't use instanceof.
  equal(Object.getPrototypeOf(map).constructor.name, "Map");

  equal(map.size, 4, "Should have 4 allocation stacks (including the lack of a stack)");

  // Gather the stacks we are expecting to appear as keys, and
  // check that there are no unexpected keys.
  let stacks = {};

  map.forEach((v, k) => {
    if (k === "noStack") {
      // No need to save this key.
    } else if (k.functionDisplayName === "f" &&
               k.parent.functionDisplayName === "test") {
      stacks.f = k;
    } else if (k.functionDisplayName === "g" &&
               k.parent.functionDisplayName === "test") {
      stacks.g = k;
    } else if (k.functionDisplayName === "h" &&
               k.parent.functionDisplayName === "test") {
      stacks.h = k;
    } else {
      dumpn("Unexpected allocation stack:");
      k.toString().split(/\n/g).forEach(s => dumpn(s));
      ok(false);
    }
  });

  ok(map.get("noStack"));
  equal(map.get("noStack").count, 1);

  ok(stacks.f);
  ok(map.get(stacks.f));
  equal(map.get(stacks.f).count, 20);

  ok(stacks.g);
  ok(map.get(stacks.g));
  equal(map.get(stacks.g).count, 10);

  ok(stacks.h);
  ok(map.get(stacks.h));
  equal(map.get(stacks.h).count, 5);

  client.destroy();
});
