/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that grid outline does not show when cells are too small to be drawn and that
// "Cannot show outline for this grid." message is displayed.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
      grid-template-columns: repeat(51, 20px);
      grid-template-rows: repeat(51, 20px);
    }
  </style>
  <div id="grid">
    <div id="cellA">cell A</div>
    <div id="cellB">cell B</div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { inspector, gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;

  await selectNode("#grid", inspector);
  const outline = doc.getElementById("grid-outline-container");
  const gridList = doc.getElementById("grid-list");
  const checkbox = gridList.children[0].querySelector("input");

  info("Toggling ON the CSS grid highlighter from the layout panel.");
  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  const onGridOutlineRendered = waitForDOM(doc, ".grid-outline-text", 1);
  const onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 1 &&
    state.grids[0].highlighted);
  checkbox.click();
  await onHighlighterShown;
  await onCheckboxChange;
  const elements = await onGridOutlineRendered;

  const cannotShowGridOutline = elements[0];

  info("Checking the grid outline is not rendered and an appropriate message is shown.");
  ok(!outline, "Outline component is not shown.");
  ok(cannotShowGridOutline,
    "The message 'Cannot show outline for this grid' is displayed.");
});
