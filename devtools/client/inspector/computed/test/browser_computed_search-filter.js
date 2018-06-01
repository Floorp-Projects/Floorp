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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openComputedView();
  await selectNode("#matches", inspector);
  await testToggleDefaultStyles(inspector, view);
  await testAddTextInFilter(inspector, view);
});

async function testToggleDefaultStyles(inspector, computedView) {
  info("checking \"Browser styles\" checkbox");
  const checkbox = computedView.includeBrowserStylesCheckbox;
  const onRefreshed = inspector.once("computed-view-refreshed");
  checkbox.click();
  await onRefreshed;
}

async function testAddTextInFilter(inspector, computedView) {
  info("setting filter text to \"color\"");
  const searchField = computedView.searchField;
  const onRefreshed = inspector.once("computed-view-refreshed");
  const win = computedView.styleWindow;

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
  await onRefreshed;

  info("check that the correct properties are visible");

  const propertyViews = computedView.propertyViews;
  propertyViews.forEach(propView => {
    const name = propView.name;
    is(propView.visible, name.indexOf("color") > -1,
      "span " + name + " property visibility check");
  });
}
