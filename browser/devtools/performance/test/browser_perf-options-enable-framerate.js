/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that `enable-framerate` toggles the visibility of the fps graph,
 * as well as enabling ticks data on the PerformanceFront.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, $ } = panel.panelWin;

  Services.prefs.setBoolPref(FRAMERATE_PREF, false);
  ok($("#time-framerate").hidden, "fps graph is hidden when ticks disabled");

  yield startRecording(panel);
  yield stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withTicks, false,
    "PerformanceFront started without ticks recording.");

  Services.prefs.setBoolPref(FRAMERATE_PREF, true);
  ok(!$("#time-framerate").hidden, "fps graph is not hidden when ticks enabled");

  yield startRecording(panel);
  yield stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withTicks, true,
    "PerformanceFront started with ticks recording.");

  yield teardown(panel);
  finish();
}
