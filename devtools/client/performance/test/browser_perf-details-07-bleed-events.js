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

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { EVENTS, DetailsView, JsCallTreeView } = panel.panelWin;

  await startRecording(panel);
  await stopRecording(panel);

  // The waterfall should render by default, and we want to make
  // sure that the render events don't bleed between detail views
  // so test that's the case after both views have been created.
  const callTreeRendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  await DetailsView.selectView("js-calltree");
  await callTreeRendered;

  const waterfallSelected = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED);
  await DetailsView.selectView("waterfall");
  await waterfallSelected;

  once(JsCallTreeView, EVENTS.UI_WATERFALL_RENDERED).then(() =>
    ok(false, "JsCallTreeView should not receive UI_WATERFALL_RENDERED event."));

  await startRecording(panel);
  await stopRecording(panel);

  const callTreeRerendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  await DetailsView.selectView("js-calltree");
  await callTreeRerendered;

  ok(true, "Test passed.");
  await teardownToolboxAndRemoveTab(panel);
});
