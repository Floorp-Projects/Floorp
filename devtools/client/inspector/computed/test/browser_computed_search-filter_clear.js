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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openComputedView();
  await selectNode("#matches", inspector);
  await testAddTextInFilter(inspector, view);
  await testClearSearchFilter(inspector, view);
});

async function testAddTextInFilter(inspector, computedView) {
  info("Setting filter text to \"background-color\"");

  const win = computedView.styleWindow;
  const propertyViews = computedView.propertyViews;
  const searchField = computedView.searchField;

  searchField.focus();
  synthesizeKeys("background-color", win);
  await inspector.once("computed-view-refreshed");

  info("Check that the correct properties are visible");

  propertyViews.forEach((propView) => {
    const name = propView.name;
    is(propView.visible, name.indexOf("background-color") > -1,
      "span " + name + " property visibility check");
  });
}

async function testClearSearchFilter(inspector, computedView) {
  info("Clearing the search filter");

  const win = computedView.styleWindow;
  const propertyViews = computedView.propertyViews;
  const searchField = computedView.searchField;
  const searchClearButton = computedView.searchClearButton;
  const onRefreshed = inspector.once("computed-view-refreshed");

  EventUtils.synthesizeMouseAtCenter(searchClearButton, {}, win);
  await onRefreshed;

  info("Check that the correct properties are visible");

  ok(!searchField.value, "Search filter is cleared");
  propertyViews.forEach((propView) => {
    is(propView.visible, propView.hasMatchedSelectors,
      "span " + propView.name + " property visibility check");
  });
}
