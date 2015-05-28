/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the details view utility functions work as advertised.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView } = panel.panelWin;
  let { PerformanceController, WaterfallView, JsCallTreeView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is selected by default in the details view.");

  let selected = DetailsView.whenViewSelected(JsCallTreeView);
  let notified = DetailsView.once(EVENTS.DETAILS_VIEW_SELECTED);
  yield DetailsView.selectView("js-calltree");
  yield Promise.all([selected, notified]);

  ok(DetailsView.isViewSelected(JsCallTreeView),
    "The jscalltree view is now selected in the details view.");

  yield PerformanceController.clearRecordings();

  yield startRecording(panel);
  let render = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  yield stopRecording(panel);
  yield render;

  ok(DetailsView.isViewSelected(JsCallTreeView),
    "The jscalltree view is still selected in the details view");

  yield teardown(panel);
  finish();
}
