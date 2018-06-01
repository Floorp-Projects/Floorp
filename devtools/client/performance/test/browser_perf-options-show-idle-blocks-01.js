/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the js flamegraphs get rerendered when toggling `show-idle-blocks`.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_SHOW_IDLE_BLOCKS_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { EVENTS, DetailsView, JsFlameGraphView } = panel.panelWin;

  Services.prefs.setBoolPref(UI_SHOW_IDLE_BLOCKS_PREF, true);

  await startRecording(panel);
  await stopRecording(panel);

  let rendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  await DetailsView.selectView("js-flamegraph");
  await rendered;

  rendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(UI_SHOW_IDLE_BLOCKS_PREF, false);
  await rendered;
  ok(true, "JsFlameGraphView rerendered when toggling show-idle-blocks.");

  rendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(UI_SHOW_IDLE_BLOCKS_PREF, true);
  await rendered;
  ok(true, "JsFlameGraphView rerendered when toggling back show-idle-blocks.");

  await teardownToolboxAndRemoveTab(panel);
});
