/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that `enable-framerate` toggles the visibility of the fps graph,
 * as well as enabling ticks data on the PerformanceFront.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, $ } = panel.panelWin;
  Services.prefs.setBoolPref(FRAMERATE_PREF, false);

  yield startRecording(panel);
  yield stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withTicks, false,
    "PerformanceFront started without ticks recording.");
  ok(!isVisible($("#time-framerate")), "fps graph is hidden when ticks disabled");

  Services.prefs.setBoolPref(FRAMERATE_PREF, true);
  ok(!isVisible($("#time-framerate")), "fps graph is still hidden if recording does not contain ticks.");

  yield startRecording(panel);
  yield stopRecording(panel);

  ok(isVisible($("#time-framerate")), "fps graph is not hidden when ticks enabled before recording");
  is(PerformanceController.getCurrentRecording().getConfiguration().withTicks, true,
    "PerformanceFront started with ticks recording.");

  yield teardown(panel);
  finish();
}
