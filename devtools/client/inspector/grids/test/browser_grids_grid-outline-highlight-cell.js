/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid cell is highlighted when mouseovering the grid outline of a
// grid cell.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div id="cella">Cell A</div>
    <div id="cellb">Cell B</div>
  </div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  let { inspector, gridInspector } = yield openLayoutView();
  let { document: doc } = gridInspector;
  let { highlighters, store } = inspector;

  yield selectNode("#grid", inspector);
  let gridList = doc.querySelector("#grid-list");
  let checkbox = gridList.children[0].querySelector("input");

  info("Toggling ON the CSS grid highlighter from the layout panel.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 1 &&
    state.grids[0].highlighted);
  checkbox.click();
  yield onHighlighterShown;
  yield onCheckboxChange;

  let gridOutline = doc.querySelector("#grid-outline");
  let gridCellA = gridOutline.children[0].querySelectorAll(
    "[data-grid-id='0'][data-grid-row='1'][data-grid-column='1']");

  info("Checking Grid Cell A and B have been rendered.");
  is(gridCellA.length, 1, "Only one grid cell A should be rendered.");

  info("Scrolling into the view the #grid-outline-container.");
  gridCellA[0].scrollIntoView();

  info("Hovering over grid cell A in the grid outline.");
  let onCellAHighlight = highlighters.once("grid-highlighter-shown",
      (event, nodeFront, options) => {
        info("Checking show grid cell options are correct.");
        const { showGridCell } = options;
        const { gridFragmentIndex, rowNumber, columnNumber } = showGridCell;

        ok(showGridCell, "Show grid cell options are available.");
        is(gridFragmentIndex, 0, "Should be the first grid fragment index.");
        is(rowNumber, 1, "Should be the first grid row.");
        is(columnNumber, 1, "Should be the first grid column.");
      });
  EventUtils.synthesizeMouse(gridCellA[0], 10, 5, {type: "mouseover"}, doc.defaultView);
  yield onCellAHighlight;
});
