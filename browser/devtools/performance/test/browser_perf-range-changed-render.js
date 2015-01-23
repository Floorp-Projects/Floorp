/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the detail views are rerendered after the range changes.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView, DetailsView } = panel.panelWin;
  let { WaterfallView, CallTreeView, FlameGraphView } = panel.panelWin;

  let updatedWaterfall = 0;
  let updatedCallTree = 0;
  let updatedFlameGraph = 0;
  WaterfallView.on(EVENTS.WATERFALL_RENDERED, () => updatedWaterfall++);
  CallTreeView.on(EVENTS.CALL_TREE_RENDERED, () => updatedCallTree++);
  FlameGraphView.on(EVENTS.FLAMEGRAPH_RENDERED, () => updatedFlameGraph++);

  yield startRecording(panel);
  yield busyWait(100);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length);
  yield stopRecording(panel);

  let rendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  OverviewView.emit(EVENTS.OVERVIEW_RANGE_SELECTED, { startTime: 0, endTime: 10 });
  yield rendered;
  ok(true, "Waterfall rerenders when a range in the overview graph is selected.");

  rendered = once(CallTreeView, EVENTS.CALL_TREE_RENDERED);
  DetailsView.selectView("calltree");
  yield rendered;
  ok(true, "Call tree rerenders after its corresponding pane is shown.");

  rendered = once(FlameGraphView, EVENTS.FLAMEGRAPH_RENDERED);
  DetailsView.selectView("flamegraph");
  yield rendered;
  ok(true, "Flamegraph rerenders after its corresponding pane is shown.");

  rendered = once(FlameGraphView, EVENTS.FLAMEGRAPH_RENDERED);
  OverviewView.emit(EVENTS.OVERVIEW_RANGE_CLEARED);
  yield rendered;
  ok(true, "Flamegraph rerenders when a range in the overview graph is removed.");

  rendered = once(CallTreeView, EVENTS.CALL_TREE_RENDERED);
  DetailsView.selectView("calltree");
  yield rendered;
  ok(true, "Call tree rerenders after its corresponding pane is shown.");

  rendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  DetailsView.selectView("waterfall");
  yield rendered;
  ok(true, "Waterfall rerenders after its corresponding pane is shown.");

  is(updatedWaterfall, 3, "WaterfallView rerendered 3 times.");
  is(updatedCallTree, 3, "CallTreeView rerendered 3 times.");
  is(updatedFlameGraph, 3, "FlameGraphView rerendered 3 times.");

  yield teardown(panel);
  finish();
}
