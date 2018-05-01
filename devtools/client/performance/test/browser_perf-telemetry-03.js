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

add_task(async function() {
  startTelemetry();

  let { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let {
    EVENTS,
    DetailsView,
    JsCallTreeView,
    JsFlameGraphView
  } = panel.panelWin;

  await startRecording(panel);
  await stopRecording(panel);

  let calltreeRendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  let flamegraphRendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);

  // Go through some views to check later.
  await DetailsView.selectView("js-calltree");
  await calltreeRendered;

  await DetailsView.selectView("js-flamegraph");
  await flamegraphRendered;

  await teardownToolboxAndRemoveTab(panel);

  checkResults();
});

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_PERFTOOLS_")
  // here.
  checkTelemetry(
    "DEVTOOLS_PERFTOOLS_SELECTED_VIEW_MS", "js-calltree", null, "hasentries");
  checkTelemetry(
    "DEVTOOLS_PERFTOOLS_SELECTED_VIEW_MS", "js-flamegraph", null, "hasentries");
  checkTelemetry(
    "DEVTOOLS_PERFTOOLS_SELECTED_VIEW_MS", "waterfall", null, "hasentries");
}
