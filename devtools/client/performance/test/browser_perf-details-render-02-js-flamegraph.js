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

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, DetailsView, JsFlameGraphView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  let rendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  yield DetailsView.selectView("js-flamegraph");
  yield rendered;

  ok(true, "JsFlameGraphView rendered after recording is stopped.");

  yield startRecording(panel);
  yield stopRecording(panel, {
    expectedViewClass: "JsFlameGraphView",
    expectedViewEvent: "UI_JS_FLAMEGRAPH_RENDERED"
  });

  ok(true, "JsFlameGraphView rendered again after recording completed a second time.");

  yield teardownToolboxAndRemoveTab(panel);
});
