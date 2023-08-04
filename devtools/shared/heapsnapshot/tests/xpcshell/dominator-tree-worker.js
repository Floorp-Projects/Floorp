/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env worker */

"use strict";

console.log("Initializing worker.");

self.onmessage = e => {
  console.log("Starting test.");
  try {
    const path = ChromeUtils.saveHeapSnapshot({ runtime: true });
    const snapshot = ChromeUtils.readHeapSnapshot(path);

    const dominatorTree = snapshot.computeDominatorTree();
    ok(dominatorTree);
    ok(DominatorTree.isInstance(dominatorTree));

    let threw = false;
    try {
      new DominatorTree();
    } catch (excp) {
      threw = true;
    }
    ok(threw, "Constructor shouldn't be usable");
  } catch (ex) {
    ok(
      false,
      "Unexpected error inside worker:\n" + ex.toString() + "\n" + ex.stack
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
