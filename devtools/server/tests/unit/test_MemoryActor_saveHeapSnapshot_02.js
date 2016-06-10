/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we can properly stream heap snapshot files over the RDP as bulk
// data.

const { OS } = require("resource://gre/modules/osfile.jsm");

const run_test = makeMemoryActorTest(function* (client, memoryFront) {
  const snapshotFilePath = yield memoryFront.saveHeapSnapshot({
    forceCopy: true
  });
  ok(!!(yield OS.File.stat(snapshotFilePath)),
     "Should have the heap snapshot file");
  const snapshot = ThreadSafeChromeUtils.readHeapSnapshot(snapshotFilePath);
  ok(snapshot instanceof HeapSnapshot,
     "And we should be able to read a HeapSnapshot instance from the file");
});
