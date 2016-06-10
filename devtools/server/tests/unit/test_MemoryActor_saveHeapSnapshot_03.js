/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we can save full runtime heap snapshots when attached to the
// ChromeActor or a ChildProcessActor.

const { OS } = require("resource://gre/modules/osfile.jsm");

const run_test = makeFullRuntimeMemoryActorTest(function* (client, memoryFront) {
  const snapshotFilePath = yield memoryFront.saveHeapSnapshot();
  ok(!!(yield OS.File.stat(snapshotFilePath)),
     "Should have the heap snapshot file");
  const snapshot = ThreadSafeChromeUtils.readHeapSnapshot(snapshotFilePath);
  ok(snapshot instanceof HeapSnapshot,
     "And we should be able to read a HeapSnapshot instance from the file");
});
