/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test the HeapAnalyses{Client,Worker} "computeDominatorTree" request.

add_task(async function() {
  const client = new HeapAnalysesClient();

  const snapshotFilePath = saveNewHeapSnapshot();
  await client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  const dominatorTreeId = await client.computeDominatorTree(snapshotFilePath);
  equal(
    typeof dominatorTreeId,
    "number",
    "should get a dominator tree id, and it should be a number"
  );

  client.destroy();
});
