/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid area and cell are highlighted when mouseovering a grid area in the
// grid outline.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
      height: 100px;
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

  info("Scrolling into the view of the #grid-outline.");
  gridCellA[0].scrollIntoView();

  info("Hovering over grid cell A in the grid outline.");
  let onCellAHighlight = highlighters.once("grid-highlighter-shown",
      (event, nodeFront, options) => {
        info("Checking the grid highlighter options for the show grid area" +
        "and cell parameters.");
        const { showGridCell, showGridArea } = options;
        const { gridFragmentIndex, rowNumber, columnNumber } = showGridCell;

        ok(showGridCell, "Show grid cell options are available.");
        ok(showGridArea, "Show grid areas options are available.");

        is(gridFragmentIndex, 0, "Should be the first grid fragment index.");
        is(rowNumber, 1, "Should be the first grid row.");
        is(columnNumber, 1, "Should be the first grid column.");

        is(showGridArea, "header", "Grid area name should be 'header'.");
      });
  EventUtils.synthesizeMouse(gridCellA[0], 10, 5, {type: "mouseover"}, doc.defaultView);
  yield onCellAHighlight;
});
