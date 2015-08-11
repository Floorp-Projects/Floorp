/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the controller handles recording via the `stopwatch` button
 * in the UI.
 */
let WAIT_TIME = 10;

function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController } = panel.panelWin;
  let front = panel.panelWin.gFront;
  loadFrameScripts();

  ok(!(yield PMM_isProfilerActive()),
    "The built-in profiler module should not have been automatically started.");

  yield startRecording(panel);
  busyWait(WAIT_TIME); // allow the profiler module to sample some cpu activity

  ok((yield PMM_isProfilerActive()),
    "The built-in profiler module should now be active.");

  yield stopRecording(panel);

  ok((yield PMM_isProfilerActive()),
    "The built-in profiler module should still be active.");

  yield teardown(panel);
  finish();
}
