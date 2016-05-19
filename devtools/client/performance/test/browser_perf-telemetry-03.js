/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the performance telemetry module records events at appropriate times.
 * Specifically the destruction of certain views.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, PerformanceController, DetailsView, JsCallTreeView, JsFlameGraphView } = panel.panelWin;

  let telemetry = PerformanceController._telemetry;
  let logs = telemetry.getLogs();
  let VIEWS = "DEVTOOLS_PERFTOOLS_SELECTED_VIEW_MS";

  yield startRecording(panel);
  yield stopRecording(panel);

  let calltreeRendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  let flamegraphRendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);

  // Go through some views to check later.
  yield DetailsView.selectView("js-calltree");
  yield calltreeRendered;

  yield DetailsView.selectView("js-flamegraph");
  yield flamegraphRendered;

  yield teardownToolboxAndRemoveTab(panel);

  // Check views after destruction to ensure `js-flamegraph` gets called
  // with a time during destruction.
  ok(logs[VIEWS].find(r => r[0] === "waterfall" && typeof r[1] === "number"), `${VIEWS} for waterfall view and time.`);
  ok(logs[VIEWS].find(r => r[0] === "js-calltree" && typeof r[1] === "number"), `${VIEWS} for js-calltree view and time.`);
  ok(logs[VIEWS].find(r => r[0] === "js-flamegraph" && typeof r[1] === "number"), `${VIEWS} for js-flamegraph view and time.`);
});
