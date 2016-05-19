/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that toggling preferences during a recording does not throw.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { DetailsView, JsCallTreeView } = panel.panelWin;

  yield DetailsView.selectView("js-calltree");
  yield startRecording(panel);

  // Manually call the _onPrefChanged function so we can catch an error.
  try {
    JsCallTreeView._onPrefChanged(null, "invert-call-tree", true);
    ok(true, "Toggling preferences during a recording should not fail.");
  } catch (e) {
    ok(false, "Toggling preferences during a recording should not fail.");
  }

  yield stopRecording(panel, {
    expectedViewClass: "JsCallTreeView",
    expectedViewEvent: "UI_JS_CALL_TREE_RENDERED"
  });

  yield teardownToolboxAndRemoveTab(panel);
});
