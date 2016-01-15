/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test other dominatorTrees can still be retrieved after deleting a snapshot

function run_test() {
  run_next_test();
}

const breakdown = {
  by: "coarseType",
  objects: { by: "count", count: true, bytes: true },
  scripts: { by: "count", count: true, bytes: true },
  strings: { by: "count", count: true, bytes: true },
  other: { by: "count", count: true, bytes: true },
};

function* createSnapshotAndDominatorTree(client) {
  let snapshotFilePath = saveNewHeapSnapshot();
  yield client.readHeapSnapshot(snapshotFilePath);
  let dominatorTreeId = yield client.computeDominatorTree(snapshotFilePath);
  return { dominatorTreeId, snapshotFilePath };
}

add_task(function* () {
  const client = new HeapAnalysesClient();

  let savedSnapshots = [
    yield createSnapshotAndDominatorTree(client),
    yield createSnapshotAndDominatorTree(client),
    yield createSnapshotAndDominatorTree(client)
  ];
  ok(true, "Create 3 snapshots and dominator trees");

  yield client.deleteHeapSnapshot(savedSnapshots[1].snapshotFilePath);
  ok(true, "Snapshot deleted");

  let tree = yield client.getDominatorTree({
    dominatorTreeId: savedSnapshots[0].dominatorTreeId,
    breakdown
  });
  ok(tree, "Should get a valid tree for first snapshot");

  let threw = false;
  try {
    yield client.getDominatorTree({
      dominatorTreeId: savedSnapshots[1].dominatorTreeId,
      breakdown
    });
  } catch (_) {
    threw = true;
  }
  ok(threw, "getDominatorTree on a deleted snapshot should throw an error");

  tree = yield client.getDominatorTree({
    dominatorTreeId: savedSnapshots[2].dominatorTreeId,
    breakdown
  });
  ok(tree, "Should get a valid tree for third snapshot");

  client.destroy();
});
