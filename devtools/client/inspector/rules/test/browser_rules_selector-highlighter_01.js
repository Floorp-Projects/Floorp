/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the selector highlighter is created when clicking on a selector
// icon in the rule view.

const TEST_URI = `
  <style type="text/css">
    body, p, td {
      background: red;
    }
  </style>
  Test the selector highlighter
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {view} = yield openRuleView();

  ok(!view.selectorHighlighter,
    "No selectorhighlighter exist in the rule-view");

  info("Clicking on a selector icon");
  let icon = yield getRuleViewSelectorHighlighterIcon(view, "body, p, td");

  let onToggled = view.once("ruleview-selectorhighlighter-toggled");
  EventUtils.synthesizeMouseAtCenter(icon, {}, view.styleWindow);
  let isVisible = yield onToggled;

  ok(view.selectorHighlighter, "The selectorhighlighter instance was created");
  ok(isVisible, "The toggle event says the highlighter is visible");
});
