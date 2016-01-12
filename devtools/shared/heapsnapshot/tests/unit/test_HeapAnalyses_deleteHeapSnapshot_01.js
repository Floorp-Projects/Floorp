/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the HeapAnalyses{Client,Worker} can delete heap snapshots.

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

add_task(function* () {
  const client = new HeapAnalysesClient();

  const snapshotFilePath = saveNewHeapSnapshot();
  yield client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  let dominatorTreeId = yield client.computeDominatorTree(snapshotFilePath);
  ok(true, "Should have computed the dominator tree");

  yield client.deleteHeapSnapshot(snapshotFilePath);
  ok(true, "Should have deleted the snapshot");

  let threw = false;
  try {
    yield client.getDominatorTree({
      dominatorTreeId: dominatorTreeId,
      breakdown
    });
  } catch (_) {
    threw = true;
  }
  ok(threw, "getDominatorTree on deleted tree should throw an error");

  threw = false;
  try {
    yield client.computeDominatorTree(snapshotFilePath);
  } catch (_) {
    threw = true;
  }
  ok(threw, "computeDominatorTree on deleted snapshot should throw an error");

  threw = false;
  try {
    yield client.takeCensus(snapshotFilePath);
  } catch (_) {
    threw = true;
  }
  ok(threw, "takeCensus on deleted tree should throw an error");

  client.destroy();
});
