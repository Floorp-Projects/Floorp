/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the js call tree views get rerendered when toggling `show-platform-data`.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_SHOW_PLATFORM_DATA_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { EVENTS, DetailsView, JsCallTreeView } = panel.panelWin;

  Services.prefs.setBoolPref(UI_SHOW_PLATFORM_DATA_PREF, true);

  await startRecording(panel);
  await stopRecording(panel);

  let rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  await DetailsView.selectView("js-calltree");
  await rendered;

  rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  Services.prefs.setBoolPref(UI_SHOW_PLATFORM_DATA_PREF, false);
  await rendered;
  ok(true, "JsCallTreeView rerendered when toggling show-idle-blocks.");

  rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  Services.prefs.setBoolPref(UI_SHOW_PLATFORM_DATA_PREF, true);
  await rendered;
  ok(true, "JsCallTreeView rerendered when toggling back show-idle-blocks.");

  await teardownToolboxAndRemoveTab(panel);
});
