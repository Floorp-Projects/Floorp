/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the shapes highlighter in the rule view with clip-path and shape-outside
// on the same element.

const TEST_URI = `
  <style type='text/css'>
    .shape {
      width: 800px;
      height: 800px;
      clip-path: circle(25%);
      shape-outside: circle(25%);
    }
  </style>
  <div class="shape" id="shape1"></div>
  <div class="shape" id="shape2"></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  const highlighters = view.highlighters;

  info("Selecting the first shapes container.");
  await selectNode("#shape1", inspector);
  let clipPathContainer = getRuleViewProperty(view, ".shape", "clip-path").valueSpan;
  let clipPathShapeToggle = clipPathContainer.querySelector(".ruleview-shapeswatch");
  let shapeOutsideContainer = getRuleViewProperty(view, ".shape",
    "shape-outside").valueSpan;
  let shapeOutsideToggle = shapeOutsideContainer.querySelector(".ruleview-shapeswatch");

  info("Toggling ON the CSS shapes highlighter for clip-path from the rule-view.");
  let onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  clipPathShapeToggle.click();
  await onHighlighterShown;
  ok(highlighters.shapesHighlighterShown, "CSS shapes highlighter is shown.");
  ok(clipPathShapeToggle.classList.contains("active"),
     "clip-path toggle button is active.");
  ok(!shapeOutsideToggle.classList.contains("active"),
     "shape-outside toggle button is not active.");

  info("Toggling ON the CSS shapes highlighter for shape-outside from the rule-view.");
  onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  shapeOutsideToggle.click();
  await onHighlighterShown;
  ok(highlighters.shapesHighlighterShown, "CSS shapes highlighter is shown.");
  ok(!clipPathShapeToggle.classList.contains("active"),
     "clip-path toggle button is not active.");
  ok(shapeOutsideToggle.classList.contains("active"),
     "shape-outside toggle button is active.");

  info("Selecting the second shapes container.");
  await selectNode("#shape2", inspector);
  clipPathContainer = getRuleViewProperty(view, ".shape", "clip-path").valueSpan;
  clipPathShapeToggle = clipPathContainer.querySelector(".ruleview-shapeswatch");
  shapeOutsideContainer = getRuleViewProperty(view, ".shape",
    "shape-outside").valueSpan;
  shapeOutsideToggle = shapeOutsideContainer.querySelector(".ruleview-shapeswatch");
  ok(!clipPathShapeToggle.classList.contains("active"),
     "clip-path toggle button is not active.");
  ok(!shapeOutsideToggle.classList.contains("active"),
     "shape-outside toggle button is not active.");
});
