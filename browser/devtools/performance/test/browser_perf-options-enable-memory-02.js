/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that toggling `enable-memory` during a recording doesn't change that
 * recording's state and does not break.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, $ } = panel.panelWin;

  // Test starting without memory, and stopping with it.
  Services.prefs.setBoolPref(MEMORY_PREF, false);
  yield startRecording(panel);

  Services.prefs.setBoolPref(MEMORY_PREF, true);
  yield stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withMemory, false,
    "The recording finished without tracking memory.");
  is(PerformanceController.getCurrentRecording().getConfiguration().withAllocations, false,
    "The recording finished without tracking allocations.");

  // Test starting with memory, and stopping without it.
  yield startRecording(panel);
  Services.prefs.setBoolPref(MEMORY_PREF, false);
  yield stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withMemory, true,
    "The recording finished with tracking memory.");
  is(PerformanceController.getCurrentRecording().getConfiguration().withAllocations, true,
    "The recording finished with tracking allocations.");

  yield teardown(panel);
  finish();
}
