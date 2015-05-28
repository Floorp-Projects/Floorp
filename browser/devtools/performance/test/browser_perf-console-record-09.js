/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that an error is not thrown when clearing out the recordings if there's
 * an in-progress console profile.
 */

function* spawnTest() {
  loadFrameScripts();
  let { target, toolbox, panel } = yield initPerformance(SIMPLE_URL);
  let win = panel.panelWin;
  let { gFront, PerformanceController } = win;

  info("Starting console.profile()...");
  yield consoleProfile(win);
  yield PerformanceController.clearRecordings();

  info("Ending console.profileEnd()...");
  consoleMethod("profileEnd");
  // Wait for the front to receive the stopped event
  yield once(gFront, "recording-stopped");

  // Wait an extra tick or two since the above promise will be resolved
  // the same time as _onRecordingStateChange, which should not cause any throwing
  yield idleWait(100);
  ok(true, "Stopping an in-progress console profile after clearing recordings does not throw.");

  yield teardown(panel);
  finish();
}
