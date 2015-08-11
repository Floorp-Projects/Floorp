/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that search filter escape keypress will clear the search field.

const TEST_URI = `
  <style type="text/css">
    .matches {
      color: #F00;
    }
  </style>
  <span id="matches" class="matches">Some styled text</span>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openComputedView();
  yield selectNode("#matches", inspector);
  yield testAddTextInFilter(inspector, view);
  yield testEscapeKeypress(inspector, view);
});

function* testAddTextInFilter(inspector, computedView) {
  info("Setting filter text to \"background-color\"");

  let win = computedView.styleWindow;
  let propertyViews = computedView.propertyViews;
  let searchField = computedView.searchField;
  let checkbox = computedView.includeBrowserStylesCheckbox;

  info("Include browser styles");
  checkbox.click();
  yield inspector.once("computed-view-refreshed");

  searchField.focus();
  synthesizeKeys("background-color", win);
  yield inspector.once("computed-view-refreshed");

  info("Check that the correct properties are visible");

  propertyViews.forEach((propView) => {
    let name = propView.name;
    is(propView.visible, name.indexOf("background-color") > -1,
      "span " + name + " property visibility check");
  });
}

function* testEscapeKeypress(inspector, computedView) {
  info("Pressing the escape key on search filter");

  let win = computedView.styleWindow;
  let propertyViews = computedView.propertyViews;
  let searchField = computedView.searchField;
  let onRefreshed = inspector.once("computed-view-refreshed");

  searchField.focus();
  EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  yield onRefreshed;

  info("Check that the correct properties are visible");

  ok(!searchField.value, "Search filter is cleared");
  propertyViews.forEach((propView) => {
    let name = propView.name;
    is(propView.visible, true,
      "span " + name + " property is visible");
  });
}
