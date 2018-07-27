/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test other dominatorTrees can still be retrieved after deleting a snapshot

const breakdown = {
  by: "coarseType",
  objects: { by: "count", count: true, bytes: true },
  scripts: { by: "count", count: true, bytes: true },
  strings: { by: "count", count: true, bytes: true },
  other: { by: "count", count: true, bytes: true },
};

async function createSnapshotAndDominatorTree(client) {
  const snapshotFilePath = saveNewHeapSnapshot();
  await client.readHeapSnapshot(snapshotFilePath);
  const dominatorTreeId = await client.computeDominatorTree(snapshotFilePath);
  return { dominatorTreeId, snapshotFilePath };
}

add_task(async function() {
  const client = new HeapAnalysesClient();

  const savedSnapshots = [
    await createSnapshotAndDominatorTree(client),
    await createSnapshotAndDominatorTree(client),
    await createSnapshotAndDominatorTree(client)
  ];
  ok(true, "Create 3 snapshots and dominator trees");

  await client.deleteHeapSnapshot(savedSnapshots[1].snapshotFilePath);
  ok(true, "Snapshot deleted");

  let tree = await client.getDominatorTree({
    dominatorTreeId: savedSnapshots[0].dominatorTreeId,
    breakdown
  });
  ok(tree, "Should get a valid tree for first snapshot");

  let threw = false;
  try {
    await client.getDominatorTree({
      dominatorTreeId: savedSnapshots[1].dominatorTreeId,
      breakdown
    });
  } catch (_) {
    threw = true;
  }
  ok(threw, "getDominatorTree on a deleted snapshot should throw an error");

  tree = await client.getDominatorTree({
    dominatorTreeId: savedSnapshots[2].dominatorTreeId,
    breakdown
  });
  ok(tree, "Should get a valid tree for third snapshot");

  client.destroy();
});
