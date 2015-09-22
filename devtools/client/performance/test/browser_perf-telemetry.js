/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the performance telemetry module records events at appropriate times.
 */

function* spawnTest() {
  PMM_loadFrameScripts(gBrowser);
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView, DetailsView, WaterfallView, JsCallTreeView, JsFlameGraphView } = panel.panelWin;

  Services.prefs.setBoolPref(MEMORY_PREF, false);
  let DURATION = "DEVTOOLS_PERFTOOLS_RECORDING_DURATION_MS";
  let COUNT = "DEVTOOLS_PERFTOOLS_RECORDING_COUNT";
  let CONSOLE_COUNT = "DEVTOOLS_PERFTOOLS_CONSOLE_RECORDING_COUNT";
  let FEATURES = "DEVTOOLS_PERFTOOLS_RECORDING_FEATURES_USED";
  let VIEWS = "DEVTOOLS_PERFTOOLS_SELECTED_VIEW_MS";
  let EXPORTED = "DEVTOOLS_PERFTOOLS_RECORDING_EXPORT_FLAG";
  let IMPORTED = "DEVTOOLS_PERFTOOLS_RECORDING_IMPORT_FLAG";

  let telemetry = PerformanceController._telemetry;
  let logs = telemetry.getLogs();

  yield startRecording(panel);
  yield stopRecording(panel);

  Services.prefs.setBoolPref(MEMORY_PREF, true);

  yield startRecording(panel);
  yield stopRecording(panel);

  is(logs[DURATION].length, 2, `two entry for ${DURATION}`);
  ok(logs[DURATION].every(d => typeof d === "number"), `every ${DURATION} entry is a number`);
  is(logs[COUNT].length, 2, `two entry for ${COUNT}`);
  is(logs[CONSOLE_COUNT], void 0, `no entries for ${CONSOLE_COUNT}`);
  is(logs[FEATURES].length, 10, `two recordings worth of entries for ${FEATURES}`);

  ok(logs[FEATURES].find(r => r[0] === "withMemory" && r[1] === true), "one feature entry for memory enabled");
  ok(logs[FEATURES].find(r => r[0] === "withMemory" && r[1] === false), "one feature entry for memory disabled");

  let calltreeRendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  let flamegraphRendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);

  // Go through some views to check later
  DetailsView.selectView("js-calltree");
  yield calltreeRendered;
  DetailsView.selectView("js-flamegraph");
  yield flamegraphRendered;

  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));
  let exported = once(PerformanceController, EVENTS.RECORDING_EXPORTED);
  yield PerformanceController.exportRecording("", PerformanceController.getCurrentRecording(), file);
  yield exported;

  ok(logs[EXPORTED], `a telemetry entry for ${EXPORTED} exists after exporting`);

  let imported = once(PerformanceController, EVENTS.RECORDING_IMPORTED);
  yield PerformanceController.importRecording(null, file);
  yield imported;

  ok(logs[IMPORTED], `a telemetry entry for ${IMPORTED} exists after importing`);

  yield consoleProfile(panel.panelWin, "rust");
  yield consoleProfileEnd(panel.panelWin, "rust");

  info("Performed a console recording.");

  is(logs[DURATION].length, 3, `three entry for ${DURATION}`);
  ok(logs[DURATION].every(d => typeof d === "number"), `every ${DURATION} entry is a number`);
  is(logs[COUNT].length, 2, `two entry for ${COUNT}`);
  is(logs[CONSOLE_COUNT].length, 1, `one entry for ${CONSOLE_COUNT}`);
  is(logs[FEATURES].length, 15, `two recordings worth of entries for ${FEATURES}`);

  yield teardown(panel);

  // Check views after destruction to ensure `js-flamegraph` gets called with a time
  // during destruction
  ok(logs[VIEWS].find(r => r[0] === "waterfall" && typeof r[1] === "number"), `${VIEWS} for waterfall view and time.`);
  ok(logs[VIEWS].find(r => r[0] === "js-calltree" && typeof r[1] === "number"), `${VIEWS} for js-calltree view and time.`);
  ok(logs[VIEWS].find(r => r[0] === "js-flamegraph" && typeof r[1] === "number"), `${VIEWS} for js-flamegraph view and time.`);

  finish();
};
