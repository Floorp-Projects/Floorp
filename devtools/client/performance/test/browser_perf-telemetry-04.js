/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the performance telemetry module records events at appropriate times.
 */

function* spawnTest() {
  // This test seems to take a long time to cleanup on Linux VMs.
  requestLongerTimeout(2);

  PMM_loadFrameScripts(gBrowser);
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView, DetailsView, WaterfallView, JsCallTreeView, JsFlameGraphView } = panel.panelWin;

  Services.prefs.setBoolPref(MEMORY_PREF, false);
  let DURATION = "DEVTOOLS_PERFTOOLS_RECORDING_DURATION_MS";
  let CONSOLE_COUNT = "DEVTOOLS_PERFTOOLS_CONSOLE_RECORDING_COUNT";
  let FEATURES = "DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED";

  let telemetry = PerformanceController._telemetry;
  let logs = telemetry.getLogs();

  yield consoleProfile(panel.panelWin, "rust");
  yield consoleProfileEnd(panel.panelWin, "rust");

  info("Performed a console recording.");

  is(logs[DURATION].length, 1, `one entry for ${DURATION}`);
  ok(logs[DURATION].every(d => typeof d === "number"), `every ${DURATION} entry is a number`);
  is(logs[CONSOLE_COUNT].length, 1, `one entry for ${CONSOLE_COUNT}`);
  is(logs[FEATURES].length, 5, `one recording worth of entries for ${FEATURES}`);

  yield teardown(panel);
  finish();
};
