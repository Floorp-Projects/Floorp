/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the swatch to toggle a shapes highlighter does not show up
// on overwritten properties.

const TEST_URI = `
  <style type='text/css'>
    #shape {
      width: 800px;
      height: 800px;
      clip-path: circle(25%);
    }
    div {
      clip-path: circle(30%);
    }
  </style>
  <div id="shape"></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();

  await selectNode("#shape", inspector);
  const container = getRuleViewProperty(view, "#shape", "clip-path").valueSpan;
  const shapeToggle = container.querySelector(".ruleview-shapeswatch");
  const shapeToggleStyle = getComputedStyle(shapeToggle);
  const overriddenContainer = getRuleViewProperty(view, "div", "clip-path").valueSpan;
  const overriddenShapeToggle =
    overriddenContainer.querySelector(".ruleview-shapeswatch");
  const overriddenShapeToggleStyle = getComputedStyle(overriddenShapeToggle);

  ok(shapeToggle && overriddenShapeToggle,
    "Shapes highlighter toggles exist in the DOM.");
  ok(!shapeToggle.classList.contains("active") &&
    !overriddenShapeToggle.classList.contains("active"),
    "Shapes highlighter toggle buttons are not active.");

  isnot(shapeToggleStyle.display, "none", "Shape highlighter toggle is not hidden");
  is(overriddenShapeToggleStyle.display, "none",
    "Overwritten shape highlighter toggle is not visible");
});
