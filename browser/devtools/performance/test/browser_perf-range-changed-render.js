/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the detail views are rerendered after the range changes.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView, DetailsView } = panel.panelWin;
  let { WaterfallView, JsCallTreeView, JsFlameGraphView } = panel.panelWin;

  let updateWaterfall = () => updatedWaterfall++;
  let updateCallTree = () => updatedCallTree++;
  let updateFlameGraph = () => updatedFlameGraph++;
  let updatedWaterfall = 0;
  let updatedCallTree = 0;
  let updatedFlameGraph = 0;
  WaterfallView.on(EVENTS.WATERFALL_RENDERED, updateWaterfall);
  JsCallTreeView.on(EVENTS.JS_CALL_TREE_RENDERED, updateCallTree);
  JsFlameGraphView.on(EVENTS.JS_FLAMEGRAPH_RENDERED, updateFlameGraph);

  yield startRecording(panel);
  yield busyWait(100);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length);
  yield stopRecording(panel);

  let rendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  OverviewView.emit(EVENTS.OVERVIEW_RANGE_SELECTED, { startTime: 0, endTime: 10 });
  yield rendered;
  ok(true, "Waterfall rerenders when a range in the overview graph is selected.");

  rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  yield DetailsView.selectView("js-calltree");
  yield rendered;
  ok(true, "Call tree rerenders after its corresponding pane is shown.");

  rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  yield DetailsView.selectView("js-flamegraph");
  yield rendered;
  ok(true, "Flamegraph rerenders after its corresponding pane is shown.");

  rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  OverviewView.emit(EVENTS.OVERVIEW_RANGE_CLEARED);
  yield rendered;
  ok(true, "Flamegraph rerenders when a range in the overview graph is removed.");

  rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  yield DetailsView.selectView("js-calltree");
  yield rendered;
  ok(true, "Call tree rerenders after its corresponding pane is shown.");

  rendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  yield DetailsView.selectView("waterfall");
  yield rendered;
  ok(true, "Waterfall rerenders after its corresponding pane is shown.");

  is(updatedWaterfall, 3, "WaterfallView rerendered 3 times.");
  is(updatedCallTree, 2, "JsCallTreeView rerendered 2 times.");
  is(updatedFlameGraph, 2, "JsFlameGraphView rerendered 2 times.");

  WaterfallView.off(EVENTS.WATERFALL_RENDERED, updateWaterfall);
  JsCallTreeView.off(EVENTS.JS_CALL_TREE_RENDERED, updateCallTree);
  JsFlameGraphView.off(EVENTS.JS_FLAMEGRAPH_RENDERED, updateFlameGraph);

  yield teardown(panel);
  finish();
}
