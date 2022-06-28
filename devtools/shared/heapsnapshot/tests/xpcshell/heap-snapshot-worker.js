/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

console.log("Initializing worker.");

self.onmessage = ex => {
  console.log("Starting test.");
  try {
    ok(ChromeUtils, "Should have access to ChromeUtils in a worker.");
    ok(HeapSnapshot, "Should have access to HeapSnapshot in a worker.");

    const filePath = ChromeUtils.saveHeapSnapshot({ globals: [this] });
    ok(true, "Should be able to save a snapshot.");

    const snapshot = ChromeUtils.readHeapSnapshot(filePath);
    ok(snapshot, "Should be able to read a heap snapshot");
    ok(
      HeapSnapshot.isInstance(snapshot),
      "Should be an instanceof HeapSnapshot"
    );
  } catch (e) {
    ok(
      false,
      "Unexpected error inside worker:\n" + e.toString() + "\n" + e.stack
    );
  } finally {
    done();
  }
};

// Proxy assertions to the main thread.
function ok(val, msg) {
  console.log("ok(" + !!val + ', "' + msg + '")');
  self.postMessage({
    type: "assertion",
    passed: !!val,
    msg,
    stack: Error().stack,
  });
}

// Tell the main thread we are done with the tests.
function done() {
  console.log("done()");
  self.postMessage({
    type: "done",
  });
}
