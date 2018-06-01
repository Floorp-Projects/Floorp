/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the grid highlighter in the rule view from an overridden 'display: grid'
// declaration.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
    div, ul {
      display: grid;
    }
  </style>
  <ul id="grid">
    <li id="cell1">cell1</li>
    <li id="cell2">cell2</li>
  </ul>
`;

const HIGHLIGHTER_TYPE = "CssGridHighlighter";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  const highlighters = view.highlighters;

  await selectNode("#grid", inspector);
  const container = getRuleViewProperty(view, "#grid", "display").valueSpan;
  const gridToggle = container.querySelector(".ruleview-grid");
  const overriddenContainer = getRuleViewProperty(view, "div, ul", "display").valueSpan;
  const overriddenGridToggle = overriddenContainer.querySelector(".ruleview-grid");

  info("Checking the initial state of the CSS grid toggle in the rule-view.");
  ok(gridToggle && overriddenGridToggle, "Grid highlighter toggles are visible.");
  ok(!gridToggle.classList.contains("active") &&
    !overriddenGridToggle.classList.contains("active"),
    "Grid highlighter toggle buttons are not active.");
  ok(!highlighters.highlighters[HIGHLIGHTER_TYPE],
    "No CSS grid highlighter exists in the rule-view.");
  ok(!highlighters.gridHighlighterShown, "No CSS grid highlighter is shown.");

  info("Toggling ON the CSS grid highlighter from the overridden rule in the rule-view.");
  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  overriddenGridToggle.click();
  await onHighlighterShown;

  info("Checking the CSS grid highlighter is created and toggle buttons are active in " +
    "the rule-view.");
  ok(gridToggle.classList.contains("active") &&
    overriddenGridToggle.classList.contains("active"),
    "Grid highlighter toggle is active.");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "CSS grid highlighter created in the rule-view.");
  ok(highlighters.gridHighlighterShown, "CSS grid highlighter is shown.");

  info("Toggling off the CSS grid highlighter from the normal grid declaration in the " +
    "rule-view.");
  const onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  gridToggle.click();
  await onHighlighterHidden;

  info("Checking the CSS grid highlighter is not shown and toggle buttons are not " +
    "active in the rule-view.");
  ok(!gridToggle.classList.contains("active") &&
    !overriddenGridToggle.classList.contains("active"),
    "Grid highlighter toggle buttons are not active.");
  ok(!highlighters.gridHighlighterShown, "No CSS grid highlighter is shown.");
});
