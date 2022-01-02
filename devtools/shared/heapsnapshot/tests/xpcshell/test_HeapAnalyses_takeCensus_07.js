/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the HeapAnalyses{Client,Worker} can take censuses and return
// an inverted CensusTreeNode.

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
  const client = new HeapAnalysesClient();

  const snapshotFilePath = saveNewHeapSnapshot();
  await client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  const { report } = await client.takeCensus(snapshotFilePath, {
    breakdown: BREAKDOWN,
  });

  const { report: treeNode } = await client.takeCensus(
    snapshotFilePath,
    {
      breakdown: BREAKDOWN,
    },
    {
      asInvertedTreeNode: true,
    }
  );

  compareCensusViewData(BREAKDOWN, report, treeNode, { invert: true });

  client.destroy();
});
