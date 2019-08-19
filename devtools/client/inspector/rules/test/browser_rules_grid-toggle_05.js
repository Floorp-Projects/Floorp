/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the grid toggle is hidden when the maximum number of grid highlighters
// have been reached.

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
  <div id="grid3" class="grid">
    <div class="cell1">cell1</div>
    <div class="cell2">cell2</div>
  </div>
`;

add_task(async function() {
  await pushPref("devtools.gridinspector.maxHighlighters", 2);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, gridInspector } = await openLayoutView();
  const ruleView = selectRuleView(inspector);
  const { document: doc } = gridInspector;
  const { highlighters } = inspector;

  await selectNode("#grid1", inspector);
  const gridList = doc.getElementById("grid-list");
  const checkbox2 = gridList.children[1].querySelector("input");
  const checkbox3 = gridList.children[2].querySelector("input");
  const container = getRuleViewProperty(ruleView, ".grid", "display").valueSpan;
  const gridToggle = container.querySelector(".ruleview-grid");

  info("Checking the initial state of the CSS grid toggle in the rule-view.");
  ok(
    !gridToggle.hasAttribute("disabled"),
    "Grid highlighter toggle is not disabled."
  );
  ok(
    !gridToggle.classList.contains("active"),
    "Grid highlighter toggle button is not active."
  );
  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");

  info("Toggling ON the CSS grid highlighter for #grid2.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  checkbox2.click();
  await onHighlighterShown;

  info(
    "Checking the CSS grid toggle for #grid1 is not disabled and not active."
  );
  ok(
    !gridToggle.hasAttribute("disabled"),
    "Grid highlighter toggle is not disabled."
  );
  ok(
    !gridToggle.classList.contains("active"),
    "Grid highlighter toggle button is not active."
  );
  is(highlighters.gridHighlighters.size, 1, "CSS grid highlighter is shown.");

  info("Toggling ON the CSS grid highlighter for #grid3.");
  onHighlighterShown = highlighters.once("grid-highlighter-shown");
  checkbox3.click();
  await onHighlighterShown;

  info("Checking the CSS grid toggle for #grid1 is disabled.");
  ok(
    gridToggle.hasAttribute("disabled"),
    "Grid highlighter toggle is disabled."
  );
  is(highlighters.gridHighlighters.size, 2, "CSS grid highlighters are shown.");

  info("Toggling OFF the CSS grid highlighter for #grid3.");
  let onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  checkbox3.click();
  await onHighlighterHidden;

  info(
    "Checking the CSS grid toggle for #grid1 is not disabled and not active."
  );
  ok(
    !gridToggle.hasAttribute("disabled"),
    "Grid highlighter toggle is not disabled."
  );
  ok(
    !gridToggle.classList.contains("active"),
    "Grid highlighter toggle button is not active."
  );
  is(highlighters.gridHighlighters.size, 1, "CSS grid highlighter is shown.");

  info("Toggling ON the CSS grid highlighter for #grid1 from the rule-view.");
  onHighlighterShown = highlighters.once("grid-highlighter-shown");
  gridToggle.click();
  await onHighlighterShown;

  info("Checking the CSS grid toggle for #grid1 is not disabled.");
  ok(
    !gridToggle.hasAttribute("disabled"),
    "Grid highlighter toggle is not disabled."
  );
  ok(
    gridToggle.classList.contains("active"),
    "Grid highlighter toggle is active."
  );
  is(highlighters.gridHighlighters.size, 2, "CSS grid highlighters are shown.");

  info("Toggling OFF the CSS grid highlighter for #grid1 from the rule-view.");
  onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  gridToggle.click();
  await onHighlighterHidden;

  info(
    "Checking the CSS grid toggle for #grid1 is not disabled and not active."
  );
  ok(
    !gridToggle.hasAttribute("disabled"),
    "Grid highlighter toggle is not disabled."
  );
  ok(
    !gridToggle.classList.contains("active"),
    "Grid highlighter toggle button is not active."
  );
  is(highlighters.gridHighlighters.size, 1, "CSS grid highlighter is shown.");
});
