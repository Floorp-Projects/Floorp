/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that when flame chart views scroll to change selection,
 * other detail views are rerendered
 */
var HORIZONTAL_AXIS = 1;
var VERTICAL_AXIS = 2;

function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView, DetailsView, WaterfallView, JsCallTreeView, JsFlameGraphView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  let waterfallRendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  let calltreeRendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  let flamegraphRendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);

  OverviewView.setTimeInterval({ startTime: 10, endTime: 20 });
  DetailsView.selectView("waterfall");
  yield waterfallRendered;
  DetailsView.selectView("js-calltree");
  yield calltreeRendered;
  DetailsView.selectView("js-flamegraph");
  yield flamegraphRendered;

  waterfallRendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  calltreeRendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  let overviewRangeSelected = once(OverviewView, EVENTS.OVERVIEW_RANGE_SELECTED);

  once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED).then(() =>
    ok(false, "FlameGraphView should not rerender, but be handled via its graph widget"));

  // Reset the range to full view, trigger a "selection" event as if
  // our mouse has done this
  scroll(JsFlameGraphView.graph, 200, HORIZONTAL_AXIS, 10);

  DetailsView.selectView("waterfall");
  yield waterfallRendered;
  ok(true, "Waterfall rerendered by flame graph changing interval");

  DetailsView.selectView("js-calltree");
  yield calltreeRendered;
  ok(true, "CallTree rerendered by flame graph changing interval");

  yield teardown(panel);
  finish();
}

// EventUtils just doesn't work!

function scroll(graph, wheel, axis, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseWheel({ testX: x, testY: y, axis, detail: wheel, axis,
    HORIZONTAL_AXIS,
    VERTICAL_AXIS
  });
}
