/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that grid highlighter is hidden on page navigation.

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

const TEST_URI_2 = "data:text/html,<html><body>test</body></html>";

ignoreGetGridsPromiseRejections();

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  const highlighters = inspector.highlighters;
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

  await navigateTo(TEST_URI_2);
  ok(!highlighters.gridHighlighters.size, "CSS grid highlighter is hidden.");
});
