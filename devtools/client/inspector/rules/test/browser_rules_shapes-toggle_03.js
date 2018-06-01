/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the shapes highlighter in the rule view with multiple shapes in the page.

const TEST_URI = `
  <style type='text/css'>
    .shape {
      width: 800px;
      height: 800px;
      clip-path: circle(25%);
    }
  </style>
  <div class="shape" id="shape1"></div>
  <div class="shape" id="shape2"></div>
`;

const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  const highlighters = view.highlighters;

  info("Selecting the first shape container.");
  await selectNode("#shape1", inspector);
  let container = getRuleViewProperty(view, ".shape", "clip-path").valueSpan;
  let shapeToggle = container.querySelector(".ruleview-shapeswatch");

  info("Checking the state of the CSS shape toggle for the first shape container " +
    "in the rule-view.");
  ok(shapeToggle, "shape highlighter toggle is visible.");
  ok(!shapeToggle.classList.contains("active"),
    "shape highlighter toggle button is not active.");
  ok(!highlighters.highlighters[HIGHLIGHTER_TYPE],
    "No CSS shape highlighter exists in the rule-view.");
  ok(!highlighters.shapesHighlighterShown, "No CSS shapes highlighter is shown.");

  info("Toggling ON the CSS shapes highlighter for the first shapes container from the " +
    "rule-view.");
  let onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  shapeToggle.click();
  await onHighlighterShown;

  info("Checking the CSS shapes highlighter is created and toggle button is active in " +
    "the rule-view.");
  ok(shapeToggle.classList.contains("active"),
    "shapes highlighter toggle is active.");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "CSS shapes highlighter created in the rule-view.");
  ok(highlighters.shapesHighlighterShown, "CSS shapes highlighter is shown.");

  info("Selecting the second shapes container.");
  await selectNode("#shape2", inspector);
  const firstShapesHighlighterShown = highlighters.shapesHighlighterShown;
  container = getRuleViewProperty(view, ".shape", "clip-path").valueSpan;
  shapeToggle = container.querySelector(".ruleview-shapeswatch");

  info("Checking the state of the CSS shapes toggle for the second shapes container " +
    "in the rule-view.");
  ok(shapeToggle, "shapes highlighter toggle is visible.");
  ok(!shapeToggle.classList.contains("active"),
    "shapes highlighter toggle button is not active.");
  ok(!highlighters.shapesHighlighterShown, "CSS shapes highlighter is still no longer" +
    "shown due to selecting another node.");

  info("Toggling ON the CSS shapes highlighter for the second shapes container " +
    "from the rule-view.");
  onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  shapeToggle.click();
  await onHighlighterShown;

  info("Checking the CSS shapes highlighter is created for the second shapes container " +
    "and toggle button is active in the rule-view.");
  ok(shapeToggle.classList.contains("active"),
    "shapes highlighter toggle is active.");
  ok(highlighters.shapesHighlighterShown != firstShapesHighlighterShown,
    "shapes highlighter for the second shapes container is shown.");

  info("Selecting the first shapes container.");
  await selectNode("#shape1", inspector);
  container = getRuleViewProperty(view, ".shape", "clip-path").valueSpan;
  shapeToggle = container.querySelector(".ruleview-shapeswatch");

  info("Checking the state of the CSS shapes toggle for the first shapes container " +
    "in the rule-view.");
  ok(shapeToggle, "shapes highlighter toggle is visible.");
  ok(!shapeToggle.classList.contains("active"),
    "shapes highlighter toggle button is not active.");
});
