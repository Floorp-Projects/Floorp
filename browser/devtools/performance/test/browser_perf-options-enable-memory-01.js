/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that `enable-memory` toggles the visibility of the memory graph,
 * as well as enabling memory data on the PerformanceFront.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, $ } = panel.panelWin;

  Services.prefs.setBoolPref(MEMORY_PREF, false);

  yield startRecording(panel);
  yield stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withMemory, false,
    "PerformanceFront started without memory recording.");
  is(PerformanceController.getCurrentRecording().getConfiguration().withAllocations, false,
    "PerformanceFront started without allocations recording.");
  ok(!isVisible($("#memory-overview")), "memory graph is hidden when memory disabled");

  Services.prefs.setBoolPref(MEMORY_PREF, true);
  ok(!isVisible($("#memory-overview")),
    "memory graph is still hidden after enabling if recording did not start recording memory");

  yield startRecording(panel);
  yield stopRecording(panel);

  ok(isVisible($("#memory-overview")), "memory graph is not hidden when memory enabled before recording");
  is(PerformanceController.getCurrentRecording().getConfiguration().withMemory, true,
    "PerformanceFront started with memory recording.");
  is(PerformanceController.getCurrentRecording().getConfiguration().withAllocations, false,
    "PerformanceFront did not record with allocations.");

  yield teardown(panel);
  finish();
}
