/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the details view toggles subviews.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");
const { command } = require("devtools/client/performance/test/helpers/input-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, $, DetailsView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  info("Checking views on startup.");
  checkViews(DetailsView, $, "waterfall");

  // Select calltree view.
  let viewChanged = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED,
                         { spreadArgs: true });
  command($("toolbarbutton[data-view='js-calltree']"));
  let [, viewName] = yield viewChanged;
  is(viewName, "js-calltree", "UI_DETAILS_VIEW_SELECTED fired with view name");
  checkViews(DetailsView, $, "js-calltree");

  // Select js flamegraph view.
  viewChanged = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED, { spreadArgs: true });
  command($("toolbarbutton[data-view='js-flamegraph']"));
  [, viewName] = yield viewChanged;
  is(viewName, "js-flamegraph", "UI_DETAILS_VIEW_SELECTED fired with view name");
  checkViews(DetailsView, $, "js-flamegraph");

  // Select waterfall view.
  viewChanged = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED, { spreadArgs: true });
  command($("toolbarbutton[data-view='waterfall']"));
  [, viewName] = yield viewChanged;
  is(viewName, "waterfall", "UI_DETAILS_VIEW_SELECTED fired with view name");
  checkViews(DetailsView, $, "waterfall");

  yield teardownToolboxAndRemoveTab(panel);
});

function checkViews(DetailsView, $, currentView) {
  for (let viewName in DetailsView.components) {
    let button = $(`toolbarbutton[data-view="${viewName}"]`);

    is(DetailsView.el.selectedPanel.id, DetailsView.components[currentView].id,
      `DetailsView correctly has ${currentView} selected.`);

    if (viewName == currentView) {
      ok(button.getAttribute("checked"), `${viewName} button checked.`);
    } else {
      ok(!button.getAttribute("checked"), `${viewName} button not checked.`);
    }
  }
}
