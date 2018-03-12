/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that events don't bleed between detail views.
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

  let { EVENTS, DetailsView, JsCallTreeView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  // The waterfall should render by default, and we want to make
  // sure that the render events don't bleed between detail views
  // so test that's the case after both views have been created.
  let callTreeRendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  yield DetailsView.selectView("js-calltree");
  yield callTreeRendered;

  let waterfallSelected = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED);
  yield DetailsView.selectView("waterfall");
  yield waterfallSelected;

  once(JsCallTreeView, EVENTS.UI_WATERFALL_RENDERED).then(() =>
    ok(false, "JsCallTreeView should not receive UI_WATERFALL_RENDERED event."));

  yield startRecording(panel);
  yield stopRecording(panel);

  let callTreeRerendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  yield DetailsView.selectView("js-calltree");
  yield callTreeRerendered;

  ok(true, "Test passed.");
  yield teardownToolboxAndRemoveTab(panel);
});
