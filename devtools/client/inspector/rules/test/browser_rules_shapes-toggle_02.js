/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the shapes highlighter in the rule view from an overridden
// declaration.

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

const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  let highlighters = view.highlighters;

  yield selectNode("#shape", inspector);
  let container = getRuleViewProperty(view, "#shape", "clip-path").valueSpan;
  let shapeToggle = container.querySelector(".ruleview-shape");
  let overriddenContainer = getRuleViewProperty(view, "div", "clip-path").valueSpan;
  let overriddenShapeToggle = overriddenContainer.querySelector(".ruleview-shape");

  info("Checking the initial state of the CSS shapes toggle in the rule-view.");
  ok(shapeToggle && overriddenShapeToggle, "Shapes highlighter toggles are visible.");
  ok(!shapeToggle.classList.contains("active") &&
    !overriddenShapeToggle.classList.contains("active"),
    "Shapes highlighter toggle buttons are not active.");
  ok(!highlighters.highlighters[HIGHLIGHTER_TYPE],
    "No CSS shapes highlighter exists in the rule-view.");
  ok(!highlighters.shapesHighlighterShown, "No CSS shapes highlighter is shown.");

  info("Toggling ON the shapes highlighter from the overridden rule in the rule-view.");
  let onHighlighterShown = highlighters.once("shapes-highlighter-shown");
  overriddenShapeToggle.click();
  yield onHighlighterShown;

  info("Checking the shapes highlighter is created and toggle buttons are active in " +
    "the rule-view.");
  ok(shapeToggle.classList.contains("active") &&
    overriddenShapeToggle.classList.contains("active"),
    "shapes highlighter toggle is active.");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "CSS shapes highlighter created in the rule-view.");
  ok(highlighters.shapesHighlighterShown, "CSS shapes highlighter is shown.");

  info("Toggling off the shapes highlighter from the normal shapes declaration in the " +
    "rule-view.");
  let onHighlighterHidden = highlighters.once("shapes-highlighter-hidden");
  shapeToggle.click();
  yield onHighlighterHidden;

  info("Checking the CSS shapes highlighter is not shown and toggle buttons are not " +
    "active in the rule-view.");
  ok(!shapeToggle.classList.contains("active") &&
    !overriddenShapeToggle.classList.contains("active"),
    "shapes highlighter toggle buttons are not active.");
  ok(!highlighters.shapesHighlighterShown, "No CSS shapes highlighter is shown.");
});
