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

add_task(async function () {
  const tab = await addTab(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
  );

  info("Check that the flexbox highlighter can be displayed.");
  await checkFlexboxHighlighter();

  info("Close the toolbox before reloading the tab.");
  await gDevTools.closeToolboxForTab(tab);

  await reloadBrowser();

  info(
    "Check that the flexbox highlighter can be displayed after reloading the page."
  );
  await checkFlexboxHighlighter();
});

async function checkFlexboxHighlighter() {
  const { inspector, view } = await openRuleView();
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.FLEXBOX;
  const { getNodeForActiveHighlighter, waitForHighlighterTypeShown } =
    getHighlighterTestHelpers(inspector);

  await selectNode("#flex", inspector);
  const container = getRuleViewProperty(view, "#flex", "display").valueSpan;
  const flexboxToggle = container.querySelector(
    ".js-toggle-flexbox-highlighter"
  );

  info("Toggling ON the flexbox highlighter from the rule-view.");
  const onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
  flexboxToggle.click();
  await onHighlighterShown;

  ok(
    getNodeForActiveHighlighter(HIGHLIGHTER_TYPE),
    "Flexbox highlighter is shown."
  );
}
