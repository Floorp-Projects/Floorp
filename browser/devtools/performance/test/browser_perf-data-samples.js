/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the retrieved profiler data samples always have a (root) node.
 * If this ever changes, the |ThreadNode.prototype.insert| function in
 * browser/devtools/profiler/utils/tree-model.js will have to be changed.
 */

const WAIT_TIME = 1000; // ms

function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let front = panel.panelWin.gFront;

  yield front.startRecording();
  busyWait(WAIT_TIME); // allow the profiler module to sample some cpu activity

  let recordingData = yield front.stopRecording();
  let profile = recordingData.profilerData.profile;

  for (let thread of profile.threads) {
    info("Checking thread: " + thread.name);

    for (let sample of thread.samples) {
      if (sample.frames[0].location != "(root)") {
        ok(false, "The sample " + sample.toSource() + " doesn't have a root node.");
      }
    }
  }

  yield teardown(panel);
  finish();
}
