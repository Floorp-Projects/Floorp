/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the js flamegraphs get rerendered when toggling `flatten-tree-recursion`.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_FLATTEN_RECURSION_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const {
    EVENTS,
    PerformanceController,
    DetailsView,
    JsFlameGraphView,
    FlameGraphUtils
  } = panel.panelWin;

  Services.prefs.setBoolPref(UI_FLATTEN_RECURSION_PREF, true);

  await startRecording(panel);
  await stopRecording(panel);

  let rendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  await DetailsView.selectView("js-flamegraph");
  await rendered;

  const thread1 = PerformanceController.getCurrentRecording().getProfile().threads[0];
  const rendering1 = FlameGraphUtils._cache.get(thread1);

  ok(thread1,
    "The samples were retrieved from the controller.");
  ok(rendering1,
    "The rendering data was cached.");

  rendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(UI_FLATTEN_RECURSION_PREF, false);
  await rendered;
  ok(true, "JsFlameGraphView rerendered when toggling flatten-tree-recursion.");

  const thread2 = PerformanceController.getCurrentRecording().getProfile().threads[0];
  const rendering2 = FlameGraphUtils._cache.get(thread2);

  is(thread1, thread2,
    "The same samples data should be retrieved from the controller (1).");
  isnot(rendering1, rendering2,
    "The rendering data should be different because other options were used (1).");

  rendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(UI_FLATTEN_RECURSION_PREF, true);
  await rendered;
  ok(true, "JsFlameGraphView rerendered when toggling back flatten-tree-recursion.");

  const thread3 = PerformanceController.getCurrentRecording().getProfile().threads[0];
  const rendering3 = FlameGraphUtils._cache.get(thread3);

  is(thread2, thread3,
    "The same samples data should be retrieved from the controller (2).");
  isnot(rendering2, rendering3,
    "The rendering data should be different because other options were used (2).");

  await teardownToolboxAndRemoveTab(panel);
});
