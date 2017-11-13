/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the flexbox highlighter in the rule view from a
// 'display: flex!important' declaration.

const TEST_URI = `
  <style type='text/css'>
    #flex {
      display: flex !important;
    }
  </style>
  <div id="flex"></div>
`;

const HIGHLIGHTER_TYPE = "FlexboxHighlighter";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  let {highlighters} = view;

  yield selectNode("#flex", inspector);
  let container = getRuleViewProperty(view, "#flex", "display").valueSpan;
  let flexboxToggle = container.querySelector(".ruleview-flex");

  info("Checking the initial state of the flexbox toggle in the rule-view.");
  ok(flexboxToggle, "Flexbox highlighter toggle is visible.");
  ok(!flexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle button is not active.");
  ok(!highlighters.highlighters[HIGHLIGHTER_TYPE],
    "No flexbox highlighter exists in the rule-view.");
  ok(!highlighters.flexboxHighlighterShown, "No flexbox highlighter is shown.");

  info("Toggling ON the flexbox highlighter from the rule-view.");
  let onHighlighterShown = highlighters.once("flexbox-highlighter-shown");
  flexboxToggle.click();
  yield onHighlighterShown;

  info("Checking the flexbox highlighter is created and toggle button is active in " +
    "the rule-view.");
  ok(flexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle is active.");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "Flexbox highlighter created in the rule-view.");
  ok(highlighters.flexboxHighlighterShown, "Flexbox highlighter is shown.");

  info("Toggling OFF the flexbox highlighter from the rule-view.");
  let onHighlighterHidden = highlighters.once("flexbox-highlighter-hidden");
  flexboxToggle.click();
  yield onHighlighterHidden;

  info("Checking the flexbox highlighter is not shown and toggle button is not active " +
    "in the rule-view.");
  ok(!flexboxToggle.classList.contains("active"),
    "Flexbox highlighter toggle button is not active.");
  ok(!highlighters.flexboxHighlighterShown, "No flexbox highlighter is shown.");
});
