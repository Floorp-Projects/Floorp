/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the selector highlighter is hidden on page navigation.

const TEST_URI = `
  <style type="text/css">
    body, p, td {
      background: red;
    }
  </style>
  Test the selector highlighter
`;

const TEST_URI_2 = "data:text/html,<html><body>test</body></html>";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  let highlighters = view.highlighters;

  info("Clicking on a selector icon");
  let icon = getRuleViewSelectorHighlighterIcon(view, "body, p, td");

  let onToggled = view.once("ruleview-selectorhighlighter-toggled");
  EventUtils.synthesizeMouseAtCenter(icon, {}, view.styleWindow);
  let isVisible = yield onToggled;

  ok(highlighters.selectorHighlighterShown, "The selectorHighlighterShown is set.");
  ok(view.selectorHighlighter, "The selectorhighlighter instance was created");
  ok(isVisible, "The toggle event says the highlighter is visible");

  yield navigateTo(inspector, TEST_URI_2);
  ok(!highlighters.selectorHighlighterShown, "The selectorHighlighterShown is unset.");
});
