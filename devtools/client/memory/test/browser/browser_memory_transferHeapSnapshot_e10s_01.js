/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can save a heap snapshot and transfer it over the RDP in e10s
// where the child process is sandboxed and so we have to use
// HeapSnapshotFileActor to get the heap snapshot file.

"use strict";

const TEST_URL = "data:text/html,<html><body></body></html>";

this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
  const memoryFront = panel.panelWin.gFront;
  ok(memoryFront, "Should get the MemoryFront");

  const snapshotFilePath = yield memoryFront.saveHeapSnapshot({
    // Force a copy so that we go through the HeapSnapshotFileActor's
    // transferHeapSnapshot request and exercise this code path on e10s.
    forceCopy: true
  });

  ok(!!(yield OS.File.stat(snapshotFilePath)),
     "Should have the heap snapshot file");

  const snapshot = ChromeUtils.readHeapSnapshot(snapshotFilePath);
  ok(snapshot instanceof HeapSnapshot,
     "And we should be able to read a HeapSnapshot instance from the file");
});
