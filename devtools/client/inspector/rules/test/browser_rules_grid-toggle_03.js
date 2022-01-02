/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the grid highlighter in the rule view with multiple grids in the page.

const TEST_URI = `
  <style type='text/css'>
    .grid {
      display: grid;
    }
  </style>
  <div id="grid1" class="grid">
    <div class="cell1">cell1</div>
    <div class="cell2">cell2</div>
  </div>
  <div id="grid2" class="grid">
    <div class="cell1">cell1</div>
    <div class="cell2">cell2</div>
  </div>
`;

add_task(async function() {
  await pushPref("devtools.gridinspector.maxHighlighters", 1);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  const highlighters = view.highlighters;
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.GRID;
  const { waitForHighlighterTypeShown } = getHighlighterTestHelpers(inspector);

  info("Selecting the first grid container.");
  await selectNode("#grid1", inspector);
  let container = getRuleViewProperty(view, ".grid", "display").valueSpan;
  let gridToggle = container.querySelector(".js-toggle-grid-highlighter");

  info(
    "Checking the state of the CSS grid toggle for the first grid container in the " +
      "rule-view."
  );
  ok(
    !gridToggle.hasAttribute("disabled"),
    "Grid highlighter toggle is not disabled."
  );
  ok(
    !gridToggle.classList.contains("active"),
    "Grid highlighter toggle button is not active."
  );
  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");

  info(
    "Toggling ON the CSS grid highlighter for the first grid container from the " +
      "rule-view."
  );
  let onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
  gridToggle.click();
  await onHighlighterShown;

  info(
    "Checking the CSS grid highlighter is created and toggle button is active in " +
      "the rule-view."
  );
  ok(
    !gridToggle.hasAttribute("disabled"),
    "Grid highlighter toggle is not disabled."
  );
  ok(
    gridToggle.classList.contains("active"),
    "Grid highlighter toggle is active."
  );
  is(highlighters.gridHighlighters.size, 1, "CSS grid highlighter is shown.");

  info("Selecting the second grid container.");
  await selectNode("#grid2", inspector);
  const firstGridHighterShown = highlighters.gridHighlighters.keys().next()
    .value;
  container = getRuleViewProperty(view, ".grid", "display").valueSpan;
  gridToggle = container.querySelector(".js-toggle-grid-highlighter");

  info(
    "Checking the state of the CSS grid toggle for the second grid container in the " +
      "rule-view."
  );
  ok(
    !gridToggle.hasAttribute("disabled"),
    "Grid highlighter toggle is not disabled."
  );
  ok(
    !gridToggle.classList.contains("active"),
    "Grid highlighter toggle button is not active."
  );
  is(
    highlighters.gridHighlighters.size,
    1,
    "CSS grid highlighter is still shown."
  );

  info(
    "Toggling ON the CSS grid highlighter for the second grid container from the " +
      "rule-view."
  );
  onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
  gridToggle.click();
  await onHighlighterShown;

  info(
    "Checking the CSS grid highlighter is created for the second grid container and " +
      "toggle button is active in the rule-view."
  );
  ok(
    gridToggle.classList.contains("active"),
    "Grid highlighter toggle is active."
  );
  ok(
    highlighters.gridHighlighters.keys().next().value != firstGridHighterShown,
    "Grid highlighter for the second grid container is shown."
  );

  info("Selecting the first grid container.");
  await selectNode("#grid1", inspector);
  container = getRuleViewProperty(view, ".grid", "display").valueSpan;
  gridToggle = container.querySelector(".js-toggle-grid-highlighter");

  info(
    "Checking the state of the CSS grid toggle for the first grid container in the " +
      "rule-view."
  );
  ok(
    !gridToggle.hasAttribute("disabled"),
    "Grid highlighter toggle is not disabled."
  );
  ok(
    !gridToggle.classList.contains("active"),
    "Grid highlighter toggle button is not active."
  );
});
