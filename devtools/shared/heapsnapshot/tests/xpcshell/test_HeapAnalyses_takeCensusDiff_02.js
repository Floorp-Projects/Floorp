/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the HeapAnalyses{Client,Worker} can take diffs between censuses as
// inverted trees.

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
  domNode: {
    by: "descriptiveType",
    then: { by: "count", count: true, bytes: true },
  },
};

add_task(async function() {
  const firstSnapshotFilePath = saveNewHeapSnapshot();
  const secondSnapshotFilePath = saveNewHeapSnapshot();

  const client = new HeapAnalysesClient();
  await client.readHeapSnapshot(firstSnapshotFilePath);
  await client.readHeapSnapshot(secondSnapshotFilePath);

  ok(true, "Should have read both heap snapshot files");

  const { delta } = await client.takeCensusDiff(
    firstSnapshotFilePath,
    secondSnapshotFilePath,
    { breakdown: BREAKDOWN }
  );

  const { delta: deltaTreeNode } = await client.takeCensusDiff(
    firstSnapshotFilePath,
    secondSnapshotFilePath,
    { breakdown: BREAKDOWN },
    { asInvertedTreeNode: true }
  );

  // Have to manually set these because symbol properties aren't structured
  // cloned.
  delta[CensusUtils.basisTotalBytes] = deltaTreeNode.totalBytes;
  delta[CensusUtils.basisTotalCount] = deltaTreeNode.totalCount;

  compareCensusViewData(BREAKDOWN, delta, deltaTreeNode, { invert: true });

  client.destroy();
});
