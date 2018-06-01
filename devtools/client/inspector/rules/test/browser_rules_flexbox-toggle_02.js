/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the flexbox highlighter in the rule view from an overridden
// 'display: flex' declaration.

const TEST_URI = `
  <style type='text/css'>
    #flex {
      display: flex;
    }
    div, ul {
      display: flex;
    }
  </style>
  <ul id="flex">
    <li>1</li>
    <li>2</li>
  </ul>
`;

const HIGHLIGHTER_TYPE = "FlexboxHighlighter";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  const {highlighters} = view;

  await selectNode("#flex", inspector);
  const container = getRuleViewProperty(view, "#flex", "display").valueSpan;
  const flexboxToggle = container.querySelector(".ruleview-flex");
  const overriddenContainer = getRuleViewProperty(view, "div, ul", "display").valueSpan;
  const overriddenFlexboxToggle = overriddenContainer.querySelector(".ruleview-flex");

  info("Checking the initial state of the flexbox toggle in the rule-view.");
  ok(flexboxToggle && overriddenFlexboxToggle,
    "Flexbox highlighter toggles are visible.");
  ok(!flexboxToggle.classList.contains("active") &&
    !overriddenFlexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle buttons are not active.");
  ok(!highlighters.highlighters[HIGHLIGHTER_TYPE],
    "No flexbox highlighter exists in the rule-view.");
  ok(!highlighters.flexboxHighlighterShown, "No flexbox highlighter is shown.");

  info("Toggling ON the flexbox highlighter from the overridden rule in the rule-view.");
  const onHighlighterShown = highlighters.once("flexbox-highlighter-shown");
  overriddenFlexboxToggle.click();
  await onHighlighterShown;

  info("Checking the flexbox highlighter is created and toggle buttons are active in " +
    "the rule-view.");
  ok(flexboxToggle.classList.contains("active") &&
    overriddenFlexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle is active.");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "Flexbox highlighter created in the rule-view.");
  ok(highlighters.flexboxHighlighterShown, "Flexbox highlighter is shown.");

  info("Toggling off the flexbox highlighter from the normal flexbox declaration in  " +
    "the rule-view.");
  const onHighlighterHidden = highlighters.once("flexbox-highlighter-hidden");
  flexboxToggle.click();
  await onHighlighterHidden;

  info("Checking the flexbox highlighter is not shown and toggle buttons are not " +
    "active in the rule-view.");
  ok(!flexboxToggle.classList.contains("active") &&
    !overriddenFlexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle buttons are not active.");
  ok(!highlighters.flexboxHighlighterShown, "No flexbox highlighter is shown.");
});
