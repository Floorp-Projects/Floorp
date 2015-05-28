/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the retrieved profiler data samples always have a (root) node.
 * If this ever changes, the |ThreadNode.prototype.insert| function in
 * browser/devtools/performance/modules/logic/tree-model.js will have to be changed.
 */

const WAIT_TIME = 1000; // ms

function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let front = panel.panelWin.gFront;

  let rec = yield front.startRecording();
  busyWait(WAIT_TIME); // allow the profiler module to sample some cpu activity

  yield front.stopRecording(rec);
  let profile = rec.getProfile();
  let sampleCount = 0;

  for (let thread of profile.threads) {
    info("Checking thread: " + thread.name);

    for (let sample of thread.samples.data) {
      sampleCount++;

      let stack = getInflatedStackLocations(thread, sample);
      if (stack[0] != "(root)") {
        ok(false, "The sample " + stack.toSource() + " doesn't have a root node.");
      }
    }
  }

  ok(sampleCount > 0,
    "At least some samples have been iterated over, checking for root nodes.");

  yield teardown(panel);
  finish();
}
