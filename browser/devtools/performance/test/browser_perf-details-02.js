/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the details view utility functions work as advertised.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView } = panel.panelWin;
  let { WaterfallView, CallTreeView, FlameGraphView } = panel.panelWin;

  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is selected by default in the details view.");

  let selected = DetailsView.whenViewSelected(CallTreeView);
  let notified = DetailsView.once(EVENTS.DETAILS_VIEW_SELECTED);
  DetailsView.selectView("calltree");
  yield Promise.all([selected, notified]);

  ok(DetailsView.isViewSelected(CallTreeView),
    "The waterfall view is now selected in the details view.");

  selected = DetailsView.whenViewSelected(FlameGraphView);
  notified = DetailsView.once(EVENTS.DETAILS_VIEW_SELECTED);
  DetailsView.selectView("flamegraph");
  yield Promise.all([selected, notified]);

  ok(DetailsView.isViewSelected(FlameGraphView),
    "The flamegraph view is now selected in the details view.");

  selected = DetailsView.whenViewSelected(WaterfallView);
  notified = DetailsView.once(EVENTS.DETAILS_VIEW_SELECTED);
  DetailsView.selectView("waterfall");
  yield Promise.all([selected, notified]);

  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is now selected in the details view.");

  yield teardown(panel);
  finish();
}
