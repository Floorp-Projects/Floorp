/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the HeapAnalyses{Client,Worker} can take censuses by
// "allocationStack" and return a CensusTreeNode.

function run_test() {
  run_next_test();
}

const BREAKDOWN = {
  by: "objectClass",
  then: {
    by: "allocationStack",
    then: { by: "count", count: true, bytes: true },
    noStack: { by: "count", count: true, bytes: true }
  },
  other: { by: "count", count: true, bytes: true }
};

add_task(function* () {
  const g = newGlobal();
  const dbg = new Debugger(g);

  // 5 allocation markers with no stack.
  g.eval(`
         this.markers = [];
         for (var i = 0; i < 5; i++) {
           markers.push(allocationMarker());
         }
         `);

  dbg.memory.allocationSamplingProbability = 1;
  dbg.memory.trackingAllocationSites = true;

  // 5 allocation markers at 5 stacks.
  g.eval(`
         (function shouldHaveCountOfOne() {
           markers.push(allocationMarker());
           markers.push(allocationMarker());
           markers.push(allocationMarker());
           markers.push(allocationMarker());
           markers.push(allocationMarker());
         }());
         `);

  // 5 allocation markers at 1 stack.
  g.eval(`
         (function shouldHaveCountOfFive() {
           for (var i = 0; i < 5; i++) {
             markers.push(allocationMarker());
           }
         }());
         `);

  const snapshotFilePath = saveNewHeapSnapshot({ debugger: dbg });

  const client = new HeapAnalysesClient();
  yield client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  const { report } = yield client.takeCensus(snapshotFilePath, {
    breakdown: BREAKDOWN
  });

  const { report: treeNode } = yield client.takeCensus(snapshotFilePath, {
    breakdown: BREAKDOWN
  }, {
    asTreeNode: true
  });

  const markers = treeNode.children.find(c => c.name === "AllocationMarker");
  ok(markers);

  const noStack = markers.children.find(c => c.name === "noStack");
  equal(noStack.count, 5);

  let numShouldHaveFiveFound = 0;
  let numShouldHaveOneFound = 0;

  function walk(node) {
    if (node.children) {
      node.children.forEach(walk);
    }

    if (!isSavedFrame(node.name)) {
      return;
    }

    if (node.name.functionDisplayName === "shouldHaveCountOfFive") {
      equal(node.count, 5, "shouldHaveCountOfFive should have count of five");
      numShouldHaveFiveFound++;
    }

    if (node.name.functionDisplayName === "shouldHaveCountOfOne") {
      equal(node.count, 1, "shouldHaveCountOfOne should have count of one");
      numShouldHaveOneFound++;
    }
  }
  markers.children.forEach(walk);

  equal(numShouldHaveFiveFound, 1);
  equal(numShouldHaveOneFound, 5);

  compareCensusViewData(BREAKDOWN, report, treeNode,
    "Returning census as a tree node represents same data as the report");

  client.destroy();
});
