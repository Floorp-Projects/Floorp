/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the search filter clear button works properly.

const TEST_URI = `
  <style type="text/css">
    .matches {
      color: #F00;
      background-color: #00F;
      border-color: #0F0;
    }
  </style>
  <span id="matches" class="matches">Some styled text</span>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openComputedView();
  yield selectNode("#matches", inspector);
  yield testAddTextInFilter(inspector, view);
  yield testClearSearchFilter(inspector, view);
});

function* testAddTextInFilter(inspector, computedView) {
  info("Setting filter text to \"background-color\"");

  let win = computedView.styleWindow;
  let propertyViews = computedView.propertyViews;
  let searchField = computedView.searchField;

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

function* testClearSearchFilter(inspector, computedView) {
  info("Clearing the search filter");

  let win = computedView.styleWindow;
  let doc = computedView.styleDocument;
  let layoutWrapper = doc.querySelector("#layout-wrapper");
  let propertyViews = computedView.propertyViews;
  let searchField = computedView.searchField;
  let searchClearButton = computedView.searchClearButton;
  let onRefreshed = inspector.once("computed-view-refreshed");

  EventUtils.synthesizeMouseAtCenter(searchClearButton, {}, win);
  yield onRefreshed;

  ok(!layoutWrapper.hidden, "Layout view is displayed");

  info("Check that the correct properties are visible");

  ok(!searchField.value, "Search filter is cleared");
  propertyViews.forEach((propView) => {
    is(propView.visible, propView.hasMatchedSelectors,
      "span " + propView.name + " property visibility check");
  });
}
