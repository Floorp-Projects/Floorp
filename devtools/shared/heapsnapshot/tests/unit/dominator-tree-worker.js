/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

console.log("Initializing worker.");

self.onmessage = e => {
  console.log("Starting test.");
  try {
    const path = ThreadSafeChromeUtils.saveHeapSnapshot({ runtime: true });
    const snapshot = ThreadSafeChromeUtils.readHeapSnapshot(path);

    const dominatorTree = snapshot.computeDominatorTree();
    ok(dominatorTree);
    ok(dominatorTree instanceof DominatorTree);

    let threw = false;
    try {
      new DominatorTree();
    } catch (e) {
      threw = true;
    }
    ok(threw, "Constructor shouldn't be usable");
  } catch (e) {
    ok(false, "Unexpected error inside worker:\n" + e.toString() + "\n" + e.stack);
  } finally {
    done();
  }
};

// Proxy assertions to the main thread.
function ok(val, msg) {
  console.log("ok(" + !!val + ", \"" + msg + "\")");
  self.postMessage({
    type: "assertion",
    passed: !!val,
    msg,
    stack: Error().stack
  });
}

// Tell the main thread we are done with the tests.
function done() {
  console.log("done()");
  self.postMessage({
    type: "done"
  });
}
