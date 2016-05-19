/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the js flamegraphs views get rerendered when toggling `invert-flame-graph`.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_INVERT_FLAME_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, DetailsView, JsFlameGraphView } = panel.panelWin;

  Services.prefs.setBoolPref(UI_INVERT_FLAME_PREF, true);

  yield startRecording(panel);
  yield stopRecording(panel);

  let rendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  yield DetailsView.selectView("js-flamegraph");
  yield rendered;

  rendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(UI_INVERT_FLAME_PREF, false);
  yield rendered;
  ok(true, "JsFlameGraphView rerendered when toggling invert-call-tree.");

  rendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(UI_INVERT_FLAME_PREF, true);
  yield rendered;
  ok(true, "JsFlameGraphView rerendered when toggling back invert-call-tree.");

  yield teardownToolboxAndRemoveTab(panel);
});
