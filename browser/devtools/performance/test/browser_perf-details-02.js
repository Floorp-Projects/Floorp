/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the details view utility functions work as advertised.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView } = panel.panelWin;
  let { WaterfallView, JsCallTreeView, JsFlameGraphView } = panel.panelWin;

  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is selected by default in the details view.");

  let selected = DetailsView.whenViewSelected(JsCallTreeView);
  let notified = DetailsView.once(EVENTS.DETAILS_VIEW_SELECTED);
  yield DetailsView.selectView("js-calltree");
  yield Promise.all([selected, notified]);

  ok(DetailsView.isViewSelected(JsCallTreeView),
    "The waterfall view is now selected in the details view.");

  selected = DetailsView.whenViewSelected(JsFlameGraphView);
  notified = DetailsView.once(EVENTS.DETAILS_VIEW_SELECTED);
  yield DetailsView.selectView("js-flamegraph");
  yield Promise.all([selected, notified]);

  ok(DetailsView.isViewSelected(JsFlameGraphView),
    "The flamegraph view is now selected in the details view.");

  selected = DetailsView.whenViewSelected(WaterfallView);
  notified = DetailsView.once(EVENTS.DETAILS_VIEW_SELECTED);
  yield DetailsView.selectView("waterfall");
  yield Promise.all([selected, notified]);

  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is now selected in the details view.");

  yield teardown(panel);
  finish();
}
