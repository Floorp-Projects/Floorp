/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the HeapAnalyses{Client,Worker} "computeDominatorTree" request.

function run_test() {
  run_next_test();
}

add_task(function* () {
  const client = new HeapAnalysesClient();

  const snapshotFilePath = saveNewHeapSnapshot();
  yield client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  const dominatorTreeId = yield client.computeDominatorTree(snapshotFilePath);
  equal(typeof dominatorTreeId, "number",
        "should get a dominator tree id, and it should be a number");

  client.destroy();
});
