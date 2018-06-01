/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the js flamegraph view renders content after recording.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { EVENTS, DetailsView, JsFlameGraphView } = panel.panelWin;

  await startRecording(panel);
  await stopRecording(panel);

  const rendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  await DetailsView.selectView("js-flamegraph");
  await rendered;

  ok(true, "JsFlameGraphView rendered after recording is stopped.");

  await startRecording(panel);
  await stopRecording(panel, {
    expectedViewClass: "JsFlameGraphView",
    expectedViewEvent: "UI_JS_FLAMEGRAPH_RENDERED"
  });

  ok(true, "JsFlameGraphView rendered again after recording completed a second time.");

  await teardownToolboxAndRemoveTab(panel);
});
