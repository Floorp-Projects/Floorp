/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the performance telemetry module records events at appropriate times.
 * Specifically the destruction of certain views.
 */

function* spawnTest() {
  // This test seems to take a long time to cleanup on Linux VMs.
  requestLongerTimeout(2);

  PMM_loadFrameScripts(gBrowser);
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView, DetailsView, WaterfallView, JsCallTreeView, JsFlameGraphView } = panel.panelWin;

  Services.prefs.setBoolPref(MEMORY_PREF, false);
  let VIEWS = "DEVTOOLS_PERFTOOLS_SELECTED_VIEW_MS";

  let telemetry = PerformanceController._telemetry;
  let logs = telemetry.getLogs();

  yield startRecording(panel);
  yield stopRecording(panel);

  let calltreeRendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  let flamegraphRendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);

  // Go through some views to check later
  DetailsView.selectView("js-calltree");
  yield calltreeRendered;
  DetailsView.selectView("js-flamegraph");
  yield flamegraphRendered;

  yield teardown(panel);

  // Check views after destruction to ensure `js-flamegraph` gets called with a time
  // during destruction
  ok(logs[VIEWS].find(r => r[0] === "waterfall" && typeof r[1] === "number"), `${VIEWS} for waterfall view and time.`);
  ok(logs[VIEWS].find(r => r[0] === "js-calltree" && typeof r[1] === "number"), `${VIEWS} for js-calltree view and time.`);
  ok(logs[VIEWS].find(r => r[0] === "js-flamegraph" && typeof r[1] === "number"), `${VIEWS} for js-flamegraph view and time.`);

  finish();
};
