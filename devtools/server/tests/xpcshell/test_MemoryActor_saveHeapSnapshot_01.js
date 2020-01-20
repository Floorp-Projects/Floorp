/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we can tell the memory actor to take a heap snapshot over the RDP
// and then create a HeapSnapshot instance from the resulting file.

const { OS } = require("resource://gre/modules/osfile.jsm");

add_task(async () => {
  const { memoryFront } = await createTabMemoryFront();

  const snapshotFilePath = await memoryFront.saveHeapSnapshot();
  ok(
    !!(await OS.File.stat(snapshotFilePath)),
    "Should have the heap snapshot file"
  );
  const snapshot = ChromeUtils.readHeapSnapshot(snapshotFilePath);
  ok(
    snapshot instanceof HeapSnapshot,
    "And we should be able to read a HeapSnapshot instance from the file"
  );
});
