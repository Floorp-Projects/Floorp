/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that flexbox highlighter is hidden on page navigation.

const TEST_URI = `
  <style type='text/css'>
    #flex {
      display: flex;
    }
  </style>
  <div id="flex"></div>
`;

const TEST_URI_2 = "data:text/html,<html><body>test</body></html>";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  let {highlighters} = view;

  yield selectNode("#flex", inspector);
  let container = getRuleViewProperty(view, "#flex", "display").valueSpan;
  let flexboxToggle = container.querySelector(".ruleview-flex");

  info("Toggling ON the flexbox highlighter from the rule-view.");
  let onHighlighterShown = highlighters.once("flexbox-highlighter-shown");
  flexboxToggle.click();
  yield onHighlighterShown;
  ok(highlighters.flexboxHighlighterShown, "Flexbox highlighter is shown.");

  yield navigateTo(inspector, TEST_URI_2);
  ok(!highlighters.flexboxHighlighterShown, "Flexbox highlighter is hidden.");
});
