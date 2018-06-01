/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the same details view is selected after recordings are cleared
 * and a new recording starts.
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

  const { EVENTS, PerformanceController, DetailsView, JsCallTreeView } = panel.panelWin;

  await startRecording(panel);
  await stopRecording(panel);

  const selected = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED);
  const rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  await DetailsView.selectView("js-calltree");
  await selected;
  await rendered;

  ok(DetailsView.isViewSelected(JsCallTreeView),
    "The js calltree view is now selected in the details view.");

  const cleared = once(PerformanceController, EVENTS.RECORDING_SELECTED, {
    expectedArgs: [null]
  });
  await PerformanceController.clearRecordings();
  await cleared;

  await startRecording(panel);
  await stopRecording(panel, {
    expectedViewClass: "JsCallTreeView",
    expectedViewEvent: "UI_JS_CALL_TREE_RENDERED"
  });

  ok(DetailsView.isViewSelected(JsCallTreeView),
    "The js calltree view is still selected in the details view.");

  await teardownToolboxAndRemoveTab(panel);
});
