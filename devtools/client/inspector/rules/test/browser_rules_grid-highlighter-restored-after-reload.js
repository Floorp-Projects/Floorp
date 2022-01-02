/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid highlighter is re-displayed after reloading a page.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
  </div>
`;

const OTHER_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
    <div id="cell3">cell3</div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  info("Check that the grid highlighter can be displayed");
  const { inspector, view } = await openRuleView();
  const { highlighters } = inspector;
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.GRID;
  const {
    waitForHighlighterTypeShown,
    waitForHighlighterTypeRestored,
    waitForHighlighterTypeDiscarded,
  } = getHighlighterTestHelpers(inspector);

  await selectNode("#grid", inspector);
  const container = getRuleViewProperty(view, "#grid", "display").valueSpan;
  const gridToggle = container.querySelector(".js-toggle-grid-highlighter");

  info("Toggling ON the CSS grid highlighter from the rule-view.");
  const onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
  gridToggle.click();
  await onHighlighterShown;

  is(highlighters.gridHighlighters.size, 1, "CSS grid highlighter is shown.");

  info("Reload the page, expect the highlighter to be displayed once again");
  const onRestored = waitForHighlighterTypeRestored(HIGHLIGHTER_TYPE);

  const onReloaded = inspector.once("reloaded");
  await reloadBrowser();
  info("Wait for inspector to be reloaded after page reload");
  await onReloaded;

  await onRestored;
  is(
    highlighters.gridHighlighters.size,
    1,
    "CSS grid highlighter was restored."
  );

  info("Navigate to another URL, and check that the highlighter is hidden");
  const otherUri =
    "data:text/html;charset=utf-8," + encodeURIComponent(OTHER_URI);
  const onDiscarded = waitForHighlighterTypeDiscarded(HIGHLIGHTER_TYPE);
  await navigateTo(otherUri);
  await onDiscarded;
  is(
    highlighters.gridHighlighters.size,
    0,
    "CSS grid highlighter was not restored."
  );
});
