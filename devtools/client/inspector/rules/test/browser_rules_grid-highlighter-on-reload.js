/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a grid highlighter showing grid gaps can be displayed after reloading the
// page (Bug 1342051).

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
      grid-gap: 10px;
    }
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
  </div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  info("Check that the grid highlighter can be displayed");
  await checkGridHighlighter();

  info("Close the toolbox before reloading the tab");
  await gDevTools.closeToolboxForTab(gBrowser.selectedTab);

  await reloadBrowser();

  info(
    "Check that the grid highlighter can be displayed after reloading the page"
  );
  await checkGridHighlighter();
});

async function checkGridHighlighter() {
  const { inspector, view } = await openRuleView();
  const { highlighters } = inspector;
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.GRID;
  const { waitForHighlighterTypeShown } = getHighlighterTestHelpers(inspector);

  await selectNode("#grid", inspector);
  const container = getRuleViewProperty(view, "#grid", "display").valueSpan;
  const gridToggle = container.querySelector(".js-toggle-grid-highlighter");

  info("Toggling ON the CSS grid highlighter from the rule-view.");
  const onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
  gridToggle.click();
  await onHighlighterShown;

  is(highlighters.gridHighlighters.size, 1, "CSS grid highlighter is shown.");
}
