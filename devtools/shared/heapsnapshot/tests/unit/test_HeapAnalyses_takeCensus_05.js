/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the HeapAnalyses{Client,Worker} can take censuses and return
// a CensusTreeNode.

function run_test() {
  run_next_test();
}

const BREAKDOWN = {
  by: "internalType",
  then: { by: "count", count: true, bytes: true }
};

add_task(function* () {
  const client = new HeapAnalysesClient();

  const snapshotFilePath = saveNewHeapSnapshot();
  yield client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  const report = yield client.takeCensus(snapshotFilePath, {
    breakdown: BREAKDOWN
  });

  const treeNode = yield client.takeCensus(snapshotFilePath, {
    breakdown: BREAKDOWN
  }, {
    asTreeNode: true
  });

  ok(treeNode.children.length > 0, "treeNode has children");
  ok(treeNode.children.every(type => {
    return "name" in type &&
           "bytes" in type &&
           "count" in type;
  }), "all of tree node's children have name, bytes, count");

  compareCensusViewData(BREAKDOWN, report, treeNode,
    "Returning census as a tree node represents same data as the report");

  client.destroy();
});
