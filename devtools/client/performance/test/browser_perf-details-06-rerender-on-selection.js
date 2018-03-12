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

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let {
    EVENTS,
    OverviewView,
    DetailsView,
    WaterfallView,
    JsCallTreeView,
    JsFlameGraphView
  } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  let waterfallRendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  OverviewView.setTimeInterval({ startTime: 10, endTime: 20 });
  yield waterfallRendered;

  // Select the call tree to make sure it's initialized and ready to receive
  // redrawing requests once reselected.
  let callTreeRendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  yield DetailsView.selectView("js-calltree");
  yield callTreeRendered;

  // Switch to the flamegraph and perform a scroll over the visualization.
  // The waterfall and call tree should get rerendered when reselected.
  let flamegraphRendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  yield DetailsView.selectView("js-flamegraph");
  yield flamegraphRendered;

  let overviewRangeSelected = once(OverviewView, EVENTS.UI_OVERVIEW_RANGE_SELECTED);
  let waterfallRerendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  let callTreeRerendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);

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

  yield overviewRangeSelected;
  ok(true, "Overview range was changed.");

  yield DetailsView.selectView("waterfall");
  yield waterfallRerendered;
  ok(true, "Waterfall rerendered by flame graph changing interval.");

  yield DetailsView.selectView("js-calltree");
  yield callTreeRerendered;
  ok(true, "CallTree rerendered by flame graph changing interval.");

  yield teardownToolboxAndRemoveTab(panel);
});
