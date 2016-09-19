/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the search filter works properly.

const TEST_URI = `
  <style type="text/css">
    .matches {
      color: #F00;
    }
  </style>
  <span id="matches" class="matches">Some styled text</span>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openComputedView();
  yield selectNode("#matches", inspector);
  yield testToggleDefaultStyles(inspector, view);
  yield testAddTextInFilter(inspector, view);
});

function* testToggleDefaultStyles(inspector, computedView) {
  info("checking \"Browser styles\" checkbox");
  let checkbox = computedView.includeBrowserStylesCheckbox;
  let onRefreshed = inspector.once("computed-view-refreshed");
  checkbox.click();
  yield onRefreshed;
}

function* testAddTextInFilter(inspector, computedView) {
  info("setting filter text to \"color\"");
  let doc = computedView.styleDocument;
  let boxModelWrapper = doc.querySelector("#boxmodel-wrapper");
  let searchField = computedView.searchField;
  let onRefreshed = inspector.once("computed-view-refreshed");
  let win = computedView.styleWindow;

  // First check to make sure that accel + F doesn't focus search if the
  // container isn't focused
  inspector.panelWin.focus();
  EventUtils.synthesizeKey("f", { accelKey: true });
  isnot(inspector.panelDoc.activeElement, searchField,
        "Search field isn't focused");

  computedView.element.focus();
  EventUtils.synthesizeKey("f", { accelKey: true });
  is(inspector.panelDoc.activeElement, searchField, "Search field is focused");

  synthesizeKeys("color", win);
  yield onRefreshed;

  ok(boxModelWrapper.hidden, "Box model is hidden");

  info("check that the correct properties are visible");

  let propertyViews = computedView.propertyViews;
  propertyViews.forEach(propView => {
    let name = propView.name;
    is(propView.visible, name.indexOf("color") > -1,
      "span " + name + " property visibility check");
  });
}
