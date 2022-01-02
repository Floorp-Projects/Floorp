/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the HeapAnalyses{Client,Worker} bubbles errors properly when things
// go wrong.

add_task(async function() {
  const client = new HeapAnalysesClient();

  // Snapshot file path to a file that doesn't exist.
  let failed = false;
  try {
    await client.readHeapSnapshot(
      getFilePath("foo-bar-baz" + Math.random(), true)
    );
  } catch (e) {
    failed = true;
  }
  ok(failed, "should not read heap snapshots that do not exist");

  // Snapshot file path to a file that is not a heap snapshot.
  failed = false;
  try {
    await client.readHeapSnapshot(
      getFilePath("test_HeapAnalyses_takeCensus_03.js")
    );
  } catch (e) {
    failed = true;
  }
  ok(
    failed,
    "should not be able to read a file " +
      "that is not a heap snapshot as a heap snapshot"
  );

  const snapshotFilePath = saveNewHeapSnapshot();
  await client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  // Bad census breakdown options.
  failed = false;
  try {
    await client.takeCensus(snapshotFilePath, {
      breakdown: { by: "some classification that we do not have" },
    });
  } catch (e) {
    failed = true;
  }
  ok(failed, "should not be able to breakdown by an unknown classification");

  client.destroy();
});
