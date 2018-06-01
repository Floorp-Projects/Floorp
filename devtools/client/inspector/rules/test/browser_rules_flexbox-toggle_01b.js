/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the flebox highlighter in the rule view and the display of the
// flexbox highlighter.

const TEST_URI = `
  <style type='text/css'>
    #flex {
      display: inline-flex;
    }
  </style>
  <div id="flex"></div>
`;

const HIGHLIGHTER_TYPE = "FlexboxHighlighter";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  const {highlighters} = view;

  await selectNode("#flex", inspector);
  const container = getRuleViewProperty(view, "#flex", "display").valueSpan;
  const flexboxToggle = container.querySelector(".ruleview-flex");

  info("Checking the initial state of the flexbox toggle in the rule-view.");
  ok(flexboxToggle, "Flexbox highlighter toggle is visible.");
  ok(!flexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle button is not active.");
  ok(!highlighters.highlighters[HIGHLIGHTER_TYPE],
    "No flexbox highlighter exists in the rule-view.");
  ok(!highlighters.flexboxHighlighterShown, "No flexbox highlighter is shown.");

  info("Toggling ON the flexbox highlighter from the rule-view.");
  const onHighlighterShown = highlighters.once("flexbox-highlighter-shown");
  flexboxToggle.click();
  await onHighlighterShown;

  info("Checking the flexbox highlighter is created and toggle button is active in " +
    "the rule-view.");
  ok(flexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle is active.");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "Flexbox highlighter created in the rule-view.");
  ok(highlighters.flexboxHighlighterShown, "Flexbox highlighter is shown.");

  info("Toggling OFF the flexbox highlighter from the rule-view.");
  const onHighlighterHidden = highlighters.once("flexbox-highlighter-hidden");
  flexboxToggle.click();
  await onHighlighterHidden;

  info("Checking the flexbox highlighter is not shown and toggle button is not active " +
    "in the rule-view.");
  ok(!flexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle button is not active.");
  ok(!highlighters.flexboxHighlighterShown, "No flexbox highlighter is shown.");
});
