/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that when flame chart views scroll to change selection,
 * other detail views are rerendered.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");
const { scrollCanvasGraph, HORIZONTAL_AXIS } = require("devtools/client/performance/test/helpers/input-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const {
    EVENTS,
    OverviewView,
    DetailsView,
    WaterfallView,
    JsCallTreeView,
    JsFlameGraphView
  } = panel.panelWin;

  await startRecording(panel);
  await stopRecording(panel);

  const waterfallRendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  OverviewView.setTimeInterval({ startTime: 10, endTime: 20 });
  await waterfallRendered;

  // Select the call tree to make sure it's initialized and ready to receive
  // redrawing requests once reselected.
  const callTreeRendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  await DetailsView.selectView("js-calltree");
  await callTreeRendered;

  // Switch to the flamegraph and perform a scroll over the visualization.
  // The waterfall and call tree should get rerendered when reselected.
  const flamegraphRendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  await DetailsView.selectView("js-flamegraph");
  await flamegraphRendered;

  const overviewRangeSelected = once(OverviewView, EVENTS.UI_OVERVIEW_RANGE_SELECTED);
  const waterfallRerendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  const callTreeRerendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);

  once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED).then(() => {
    ok(false, "FlameGraphView should not publicly rerender, the internal state " +
              "and drawing should be handled by the underlying widget.");
  });

  // Reset the range to full view, trigger a "selection" event as if
  // our mouse has done this
  scrollCanvasGraph(JsFlameGraphView.graph, {
    axis: HORIZONTAL_AXIS,
    wheel: 200,
    x: 10
  });

  await overviewRangeSelected;
  ok(true, "Overview range was changed.");

  await DetailsView.selectView("waterfall");
  await waterfallRerendered;
  ok(true, "Waterfall rerendered by flame graph changing interval.");

  await DetailsView.selectView("js-calltree");
  await callTreeRerendered;
  ok(true, "CallTree rerendered by flame graph changing interval.");

  await teardownToolboxAndRemoveTab(panel);
});
