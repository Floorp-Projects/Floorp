/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the waterfall view renders content after recording.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, DetailsView, WaterfallView } = panel.panelWin;

  yield startRecording(panel);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length);

  let rendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  yield stopRecording(panel);
  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is selected by default in the details view.");
  yield rendered;

  ok(true, "WaterfallView rendered after recording is stopped.");

  yield startRecording(panel);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length);

  rendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  ok(true, "WaterfallView rendered again after recording completed a second time.");

  yield teardown(panel);
  finish();
}
