/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the performance telemetry module records events at appropriate times.
 * Specificaly the state about a recording process.
 */

function* spawnTest() {
  // This test seems to take a long time to cleanup on Linux VMs.
  requestLongerTimeout(2);

  PMM_loadFrameScripts(gBrowser);
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView, DetailsView, WaterfallView, JsCallTreeView, JsFlameGraphView } = panel.panelWin;

  Services.prefs.setBoolPref(MEMORY_PREF, false);
  let DURATION = "DEVTOOLS_PERFTOOLS_RECORDING_DURATION_MS";
  let COUNT = "DEVTOOLS_PERFTOOLS_RECORDING_COUNT";
  let CONSOLE_COUNT = "DEVTOOLS_PERFTOOLS_CONSOLE_RECORDING_COUNT";
  let FEATURES = "DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED";

  let telemetry = PerformanceController._telemetry;
  let logs = telemetry.getLogs();

  yield startRecording(panel);
  yield stopRecording(panel);

  Services.prefs.setBoolPref(MEMORY_PREF, true);

  yield startRecording(panel);
  yield stopRecording(panel);

  is(logs[DURATION].length, 2, `two entries for ${DURATION}`);
  ok(logs[DURATION].every(d => typeof d === "number"), `every ${DURATION} entry is a number`);
  is(logs[COUNT].length, 2, `two entries for ${COUNT}`);
  is(logs[CONSOLE_COUNT], void 0, `no entries for ${CONSOLE_COUNT}`);
  is(logs[FEATURES].length, 10, `two recordings worth of entries for ${FEATURES}`);
  ok(logs[FEATURES].find(r => r[0] === "withMemory" && r[1] === true), "one feature entries for memory enabled");
  ok(logs[FEATURES].find(r => r[0] === "withMemory" && r[1] === false), "one feature entries for memory disabled");

  yield teardown(panel);
  finish();
};
