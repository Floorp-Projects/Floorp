/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the performance telemetry module records events at appropriate times.
 * Specifically export/import.
 */

function* spawnTest() {
  // This test seems to take a long time to cleanup on Linux VMs.
  requestLongerTimeout(2);

  PMM_loadFrameScripts(gBrowser);
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView, DetailsView, WaterfallView, JsCallTreeView, JsFlameGraphView } = panel.panelWin;

  Services.prefs.setBoolPref(MEMORY_PREF, false);
  let EXPORTED = "DEVTOOLS_PERFTOOLS_RECORDING_EXPORT_FLAG";
  let IMPORTED = "DEVTOOLS_PERFTOOLS_RECORDING_IMPORT_FLAG";

  let telemetry = PerformanceController._telemetry;
  let logs = telemetry.getLogs();

  yield startRecording(panel);
  yield stopRecording(panel);

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

  yield teardown(panel);
  finish();
};
