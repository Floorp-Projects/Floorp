/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the HeapAnalyses{Client,Worker} can delete heap snapshots.

const breakdown = {
  by: "coarseType",
  objects: { by: "count", count: true, bytes: true },
  scripts: { by: "count", count: true, bytes: true },
  strings: { by: "count", count: true, bytes: true },
  other: { by: "count", count: true, bytes: true },
};

add_task(async function() {
  const client = new HeapAnalysesClient();

  const snapshotFilePath = saveNewHeapSnapshot();
  await client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  const dominatorTreeId = await client.computeDominatorTree(snapshotFilePath);
  ok(true, "Should have computed the dominator tree");

  await client.deleteHeapSnapshot(snapshotFilePath);
  ok(true, "Should have deleted the snapshot");

  let threw = false;
  try {
    await client.getDominatorTree({
      dominatorTreeId,
      breakdown,
    });
  } catch (_) {
    threw = true;
  }
  ok(threw, "getDominatorTree on deleted tree should throw an error");

  threw = false;
  try {
    await client.computeDominatorTree(snapshotFilePath);
  } catch (_) {
    threw = true;
  }
  ok(threw, "computeDominatorTree on deleted snapshot should throw an error");

  threw = false;
  try {
    await client.takeCensus(snapshotFilePath);
  } catch (_) {
    threw = true;
  }
  ok(threw, "takeCensus on deleted tree should throw an error");

  client.destroy();
});
