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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  info("Check that the flexbox highlighter can be displayed.");
  await checkFlexboxHighlighter();

  info("Close the toolbox before reloading the tab.");
  const target = TargetFactory.forTab(gBrowser.selectedTab);
  await gDevTools.closeToolbox(target);

  await refreshTab();

  info("Check that the flexbox highlighter can be displayed after reloading the page.");
  await checkFlexboxHighlighter();
});

async function checkFlexboxHighlighter() {
  const {inspector, view} = await openRuleView();
  const {highlighters} = view;

  await selectNode("#flex", inspector);
  const container = getRuleViewProperty(view, "#flex", "display").valueSpan;
  const flexboxToggle = container.querySelector(".ruleview-flex");

  info("Toggling ON the flexbox highlighter from the rule-view.");
  const onHighlighterShown = highlighters.once("flexbox-highlighter-shown");
  flexboxToggle.click();
  await onHighlighterShown;

  ok(highlighters.flexboxHighlighterShown, "Flexbox highlighter is shown.");
}
