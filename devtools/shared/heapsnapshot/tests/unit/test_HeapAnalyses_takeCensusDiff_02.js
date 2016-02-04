/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the HeapAnalyses{Client,Worker} can take diffs between censuses as
// inverted trees.

function run_test() {
  run_next_test();
}

const BREAKDOWN = {
  by: "coarseType",
  objects: {
    by: "objectClass",
    then: { by: "count", count: true, bytes: true },
    other: { by: "count", count: true, bytes: true },
  },
  scripts: {
    by: "internalType",
    then: { by: "count", count: true, bytes: true },
  },
  strings: {
    by: "internalType",
    then: { by: "count", count: true, bytes: true },
  },
  other: {
    by: "internalType",
    then: { by: "count", count: true, bytes: true },
  },
};

add_task(function* () {
  const firstSnapshotFilePath = saveNewHeapSnapshot();
  const secondSnapshotFilePath = saveNewHeapSnapshot();

  const client = new HeapAnalysesClient();
  yield client.readHeapSnapshot(firstSnapshotFilePath);
  yield client.readHeapSnapshot(secondSnapshotFilePath);

  ok(true, "Should have read both heap snapshot files");

  const { delta } = yield client.takeCensusDiff(firstSnapshotFilePath,
                                                secondSnapshotFilePath,
                                                { breakdown: BREAKDOWN });

  const { delta: deltaTreeNode } = yield client.takeCensusDiff(firstSnapshotFilePath,
                                                               secondSnapshotFilePath,
                                                               { breakdown: BREAKDOWN },
                                                               { asInvertedTreeNode: true });

  // Have to manually set these because symbol properties aren't structured
  // cloned.
  delta[CensusUtils.basisTotalBytes] = deltaTreeNode.totalBytes;
  delta[CensusUtils.basisTotalCount] = deltaTreeNode.totalCount;

  compareCensusViewData(BREAKDOWN, delta, deltaTreeNode, { invert: true });

  client.destroy();
});
