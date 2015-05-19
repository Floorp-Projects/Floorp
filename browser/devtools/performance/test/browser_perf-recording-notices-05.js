/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that when a recording overlaps the circular buffer, that
 * a class is assigned to the recording notices.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL, void 0, { TEST_MOCK_PROFILER_CHECK_TIMER: 10 });
  let { EVENTS, $, PerformanceController, PerformanceView } = panel.panelWin;

  let supported = false;
  let enabled = false;

  PerformanceController.getMultiprocessStatus = () => {
    return { supported, enabled };
  };

  PerformanceController._setMultiprocessAttributes();
  ok($("#performance-view").getAttribute("e10s"), "unsupported",
    "when e10s is disabled and no option to turn on, container has [e10s=unsupported]");

  supported = true;
  enabled = false;
  PerformanceController._setMultiprocessAttributes();
  ok($("#performance-view").getAttribute("e10s"), "disabled",
    "when e10s is disabled and but is supported, container has [e10s=disabled]");

  supported = false;
  enabled = true;
  PerformanceController._setMultiprocessAttributes();
  ok($("#performance-view").getAttribute("e10s"), "",
    "when e10s is enabled, but not supported, this probably means we no longer have E10S_TESTING_ONLY, and we have no e10s attribute.");

  supported = true;
  enabled = true;
  PerformanceController._setMultiprocessAttributes();
  ok($("#performance-view").getAttribute("e10s"), "",
    "when e10s is enabled and supported, there should be no e10s attribute.");

  yield teardown(panel);
  finish();
}
