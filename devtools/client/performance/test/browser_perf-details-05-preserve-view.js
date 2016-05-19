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

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, PerformanceController, DetailsView, JsCallTreeView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  let selected = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED);
  let rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  yield DetailsView.selectView("js-calltree");
  yield selected;
  yield rendered;

  ok(DetailsView.isViewSelected(JsCallTreeView),
    "The js calltree view is now selected in the details view.");

  let cleared = once(PerformanceController, EVENTS.RECORDING_SELECTED, { expectedArgs: { "1": null } });
  yield PerformanceController.clearRecordings();
  yield cleared;

  yield startRecording(panel);
  yield stopRecording(panel, {
    expectedViewClass: "JsCallTreeView",
    expectedViewEvent: "UI_JS_CALL_TREE_RENDERED"
  });

  ok(DetailsView.isViewSelected(JsCallTreeView),
    "The js calltree view is still selected in the details view.");

  yield teardownToolboxAndRemoveTab(panel);
});
