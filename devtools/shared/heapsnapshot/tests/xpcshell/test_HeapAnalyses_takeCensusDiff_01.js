/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the HeapAnalyses{Client,Worker} can take diffs between censuses.

const BREAKDOWN = {
  by: "objectClass",
  then: { by: "count", count: true, bytes: false },
  other: { by: "count", count: true, bytes: false },
};

add_task(async function () {
  const client = new HeapAnalysesClient();

  const markers = [allocationMarker()];

  const firstSnapshotFilePath = saveNewHeapSnapshot();

  // Allocate and hold an additional AllocationMarker object so we can see it in
  // the next heap snapshot.
  markers.push(allocationMarker());

  const secondSnapshotFilePath = saveNewHeapSnapshot();

  await client.readHeapSnapshot(firstSnapshotFilePath);
  await client.readHeapSnapshot(secondSnapshotFilePath);
  ok(true, "Should have read both heap snapshot files");

  const { delta } = await client.takeCensusDiff(
    firstSnapshotFilePath,
    secondSnapshotFilePath,
    { breakdown: BREAKDOWN }
  );

  equal(
    delta.AllocationMarker.count,
    1,
    "There exists one new AllocationMarker in the second heap snapshot"
  );

  const { delta: deltaTreeNode } = await client.takeCensusDiff(
    firstSnapshotFilePath,
    secondSnapshotFilePath,
    { breakdown: BREAKDOWN },
    { asTreeNode: true }
  );

  // Have to manually set these because symbol properties aren't structured
  // cloned.
  delta[CensusUtils.basisTotalBytes] = deltaTreeNode.totalBytes;
  delta[CensusUtils.basisTotalCount] = deltaTreeNode.totalCount;

  compareCensusViewData(
    BREAKDOWN,
    delta,
    deltaTreeNode,
    "Returning delta-census as a tree node represents same data as the report"
  );

  client.destroy();
});
