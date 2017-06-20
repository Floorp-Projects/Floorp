/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that grid outline does not show when cells are too small to be drawn and that
// "Cannot show outline for this grid." message is displayed.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
      grid-template-columns: 2px;
    }
    .cell {
      grid-template-columns: 2px;
    }
  </style>
  <div id="grid">
    <div id="cellA" className="cell">cell A</div>
    <div id="cellB" className="cell">cell B</div>
  </div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  let { inspector, gridInspector } = yield openLayoutView();
  let { document: doc } = gridInspector;
  let { highlighters, store } = inspector;

  yield selectNode("#grid", inspector);
  let outline = doc.getElementById("grid-outline-container");
  let gridList = doc.getElementById("grid-list");
  let checkbox = gridList.children[0].querySelector("input");

  info("Toggling ON the CSS grid highlighter from the layout panel.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onGridOutlineRendered = waitForDOM(doc, ".grid-outline-text", 1);
  let onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 1 &&
    state.grids[0].highlighted);
  checkbox.click();
  yield onHighlighterShown;
  yield onCheckboxChange;
  let elements = yield onGridOutlineRendered;

  let cannotShowGridOutline = elements[0];

  info("Checking the grid outline is not rendered and an appropriate message is shown.");
  ok(!outline, "Outline component is not shown.");
  ok(cannotShowGridOutline,
    "The message 'Cannot show outline for this grid' is displayed.");
});
