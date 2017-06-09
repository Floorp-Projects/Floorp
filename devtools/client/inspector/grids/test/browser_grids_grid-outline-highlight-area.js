/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid area and cell are highlighted when hovering over a grid area in the
// grid outline.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
      grid-template-areas:
        "header"
        "footer";
    }
    .top {
      grid-area: header;
    }
    .bottom {
      grid-area: footer;
    }
  </style>
  <div id="grid">
    <div id="cella" className="top">Cell A</div>
    <div id="cellb" className="bottom">Cell B</div>
  </div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  let { inspector, gridInspector } = yield openLayoutView();
  let { document: doc } = gridInspector;
  let { highlighters, store } = inspector;

  // Don't track reflows since this might cause intermittent failures.
  inspector.reflowTracker.untrackReflows(gridInspector, gridInspector.onReflow);

  let gridList = doc.getElementById("grid-list");
  let checkbox = gridList.children[0].querySelector("input");

  info("Toggling ON the CSS grid highlighter from the layout panel.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onGridOutlineRendered = waitForDOM(doc, "#grid-cell-group rect", 3);
  let onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 1 &&
    state.grids[0].highlighted);
  checkbox.click();
  yield onCheckboxChange;
  yield onHighlighterShown;
  let elements = yield onGridOutlineRendered;

  let gridCellA = elements[1];

  info("Hovering over grid cell A in the grid outline.");
  let onCellAHighlight = highlighters.once("grid-highlighter-shown",
    (event, nodeFront, options) => {
      info("Checking the grid highlighter options for the show grid area" +
      "and cell parameters.");
      const { showGridCell, showGridArea } = options;
      const { gridFragmentIndex, rowNumber, columnNumber } = showGridCell;

      is(gridFragmentIndex, 0, "Should be the first grid fragment index.");
      is(rowNumber, 1, "Should be the first grid row.");
      is(columnNumber, 1, "Should be the first grid column.");
      is(showGridArea, "header", "Grid area name should be 'header'.");
    });
  EventUtils.synthesizeMouse(gridCellA, 5, 5, {type: "mouseover"}, doc.defaultView);
  yield onCellAHighlight;
});
