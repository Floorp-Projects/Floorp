/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the HeapAnalyses{Client,Worker} can read heap snapshots.

add_task(async function () {
  const client = new HeapAnalysesClient();

  const snapshotFilePath = saveNewHeapSnapshot();
  await client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  client.destroy();
});
