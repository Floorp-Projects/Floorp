/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the HeapAnalyses{Client,Worker} can take censuses and return
// a CensusTreeNode.

const BREAKDOWN = {
  by: "internalType",
  then: { by: "count", count: true, bytes: true },
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
      asTreeNode: true,
    }
  );

  ok(!!treeNode.children.length, "treeNode has children");
  ok(
    treeNode.children.every(type => {
      return "name" in type && "bytes" in type && "count" in type;
    }),
    "all of tree node's children have name, bytes, count"
  );

  compareCensusViewData(
    BREAKDOWN,
    report,
    treeNode,
    "Returning census as a tree node represents same data as the report"
  );

  client.destroy();
});
