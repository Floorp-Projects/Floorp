/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the details view toggles subviews.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, DetailsView, document: doc } = panel.panelWin;

  info("views on startup");
  checkViews(DetailsView, doc, "waterfall");

  // Select calltree view
  let viewChanged = onceSpread(DetailsView, EVENTS.DETAILS_VIEW_SELECTED);
  command($("toolbarbutton[data-view='calltree']"));
  let [_, viewName] = yield viewChanged;
  is(viewName, "calltree", "DETAILS_VIEW_SELECTED fired with view name");
  checkViews(DetailsView, doc, "calltree");

  // Select waterfall view
  viewChanged = onceSpread(DetailsView, EVENTS.DETAILS_VIEW_SELECTED);
  command($("toolbarbutton[data-view='waterfall']"));
  [_, viewName] = yield viewChanged;
  is(viewName, "waterfall", "DETAILS_VIEW_SELECTED fired with view name");
  checkViews(DetailsView, doc, "waterfall");

  // Select flamegraph view
  viewChanged = onceSpread(DetailsView, EVENTS.DETAILS_VIEW_SELECTED);
  command($("toolbarbutton[data-view='flamegraph']"));
  [_, viewName] = yield viewChanged;
  is(viewName, "flamegraph", "DETAILS_VIEW_SELECTED fired with view name");
  checkViews(DetailsView, doc, "flamegraph");

  yield teardown(panel);
  finish();
}

function checkViews (DetailsView, doc, currentView) {
  for (let viewName in DetailsView.viewIndexes) {
    let button = doc.querySelector(`toolbarbutton[data-view="${viewName}"]`);

    is(DetailsView.el.selectedIndex, DetailsView.viewIndexes[currentView],
      `DetailsView correctly has ${currentView} selected.`);
    if (viewName === currentView) {
      ok(button.getAttribute("checked"), `${viewName} button checked`);
    } else {
      ok(!button.getAttribute("checked"), `${viewName} button not checked`);
    }
  }
}
