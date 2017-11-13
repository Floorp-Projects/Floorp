/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a flexbox highlighter after reloading the page.

const TEST_URI = `
  <style type='text/css'>
    #flex {
      display: flex;
    }
  </style>
  <div id="flex"></div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  info("Check that the flexbox highlighter can be displayed.");
  yield checkFlexboxHighlighter();

  info("Close the toolbox before reloading the tab.");
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.closeToolbox(target);

  yield refreshTab(gBrowser.selectedTab);

  info("Check that the flexbox highlighter can be displayed after reloading the page.");
  yield checkFlexboxHighlighter();
});

function* checkFlexboxHighlighter() {
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
}
