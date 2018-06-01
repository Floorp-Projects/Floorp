/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Avoid test timeouts on Linux debug builds where the test takes just a bit too long to
// run (see bug 1258081).
requestLongerTimeout(2);

// Tests that search filter escape keypress will clear the search field.

const TEST_URI = `
  <style type="text/css">
    .matches {
      color: #F00;
    }
  </style>
  <span id="matches" class="matches">Some styled text</span>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openComputedView();
  await selectNode("#matches", inspector);
  await testAddTextInFilter(inspector, view);
  await testEscapeKeypress(inspector, view);
});

async function testAddTextInFilter(inspector, computedView) {
  info("Setting filter text to \"background-color\"");

  const win = computedView.styleWindow;
  const propertyViews = computedView.propertyViews;
  const searchField = computedView.searchField;
  const checkbox = computedView.includeBrowserStylesCheckbox;

  info("Include browser styles");
  checkbox.click();
  await inspector.once("computed-view-refreshed");

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

async function testEscapeKeypress(inspector, computedView) {
  info("Pressing the escape key on search filter");

  const win = computedView.styleWindow;
  const propertyViews = computedView.propertyViews;
  const searchField = computedView.searchField;
  const onRefreshed = inspector.once("computed-view-refreshed");

  searchField.focus();
  EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  await onRefreshed;

  info("Check that the correct properties are visible");

  ok(!searchField.value, "Search filter is cleared");
  propertyViews.forEach((propView) => {
    const name = propView.name;
    is(propView.visible, true,
      "span " + name + " property is visible");
  });
}
