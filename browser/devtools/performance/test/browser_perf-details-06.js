/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the views with `shouldUpdateWhileMouseIsActive` works as intended.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView, DetailsView, WaterfallView, JsFlameGraphView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  // Set the debounce on WaterfallView and JsFlameGraphView to 0
  WaterfallView.rangeChangeDebounceTime = 0;
  JsFlameGraphView.rangeChangeDebounceTime = 0;

  yield DetailsView.selectView("js-flamegraph");
  let duration = PerformanceController.getCurrentRecording().getDuration();

  // Fake an active mouse
  Object.defineProperty(OverviewView, "isMouseActive", { value: true });

  // Flame Graph should update on every selection, debounced, while mouse is down
  let flamegraphRendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  OverviewView.emit(EVENTS.OVERVIEW_RANGE_SELECTED, { startTime: 0, endTime: duration });
  yield flamegraphRendered;
  ok(true, "FlameGraph rerenders when mouse is active (1)");

  flamegraphRendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  OverviewView.emit(EVENTS.OVERVIEW_RANGE_SELECTED, { startTime: 0, endTime: duration });
  yield flamegraphRendered;
  ok(true, "FlameGraph rerenders when mouse is active (2)");

  ok(OverviewView.isMouseActive, "Fake mouse is still active");

  // Fake an inactive mouse for rerender
  Object.defineProperty(OverviewView, "isMouseActive", { value: false });
  yield DetailsView.selectView("waterfall");

  // Fake an active mouse for rerender
  Object.defineProperty(OverviewView, "isMouseActive", { value: true });

  let oneSecondElapsed = false;
  let waterfallRendered = false;

  WaterfallView.on(EVENTS.WATERFALL_RENDERED, () => waterfallRendered = true);

  // Keep firing range selection events for one second
  idleWait(1000).then(() => oneSecondElapsed = true);
  yield waitUntil(() => {
    OverviewView.emit(EVENTS.OVERVIEW_RANGE_SELECTED, { startTime: 0, endTime: duration });
    return oneSecondElapsed;
  });

  ok(OverviewView.isMouseActive, "Fake mouse is still active");
  ok(!waterfallRendered, "the waterfall view should not have been rendered while mouse is active.");

  yield teardown(panel);
  finish();
}
