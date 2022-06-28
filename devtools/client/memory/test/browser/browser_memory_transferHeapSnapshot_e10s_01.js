/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* global ChromeUtils, HeapSnapshot */

// Test that we can save a heap snapshot and transfer it over the RDP in e10s
// where the child process is sandboxed and so we have to use
// HeapSnapshotFileActor to get the heap snapshot file.

"use strict";

const TEST_URL = "data:text/html,<html><body></body></html>";

this.test = makeMemoryTest(TEST_URL, async function({ tab, panel }) {
  const memoryFront = panel.panelWin.gStore.getState().front;
  ok(memoryFront, "Should get the MemoryFront");

  const snapshotFilePath = await memoryFront.saveHeapSnapshot({
    // Force a copy so that we go through the HeapSnapshotFileActor's
    // transferHeapSnapshot request and exercise this code path on e10s.
    forceCopy: true,
  });

  ok(
    !!(await IOUtils.stat(snapshotFilePath)),
    "Should have the heap snapshot file"
  );

  const snapshot = ChromeUtils.readHeapSnapshot(snapshotFilePath);
  ok(
    HeapSnapshot.isInstance(snapshot),
    "And we should be able to read a HeapSnapshot instance from the file"
  );
});
