/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid cell is highlighted when hovering over the grid outline of a
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

ignoreGetGridsPromiseRejections();

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { inspector, gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { store } = inspector;
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.GRID;
  const { waitForHighlighterTypeShown } = getHighlighterTestHelpers(inspector);

  const gridList = doc.getElementById("grid-list");
  const checkbox = gridList.children[0].querySelector("input");

  info("Toggling ON the CSS grid highlighter from the layout panel.");
  const onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
  const onGridOutlineRendered = waitForDOM(doc, "#grid-cell-group rect", 2);
  const onCheckboxChange = waitUntilState(
    store,
    state => state.grids.length == 1 && state.grids[0].highlighted
  );
  checkbox.click();

  info("Wait for checkbox to change");
  await onCheckboxChange;

  info("Wait for highlighter to be shown");
  await onHighlighterShown;

  info("Wait for outline to be rendered");
  await onGridOutlineRendered;

  info("Hovering over grid cell A in the grid outline.");
  const onCellAHighlight = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);

  synthesizeMouseOverOnGridCell(doc, 0);

  const { options } = await onCellAHighlight;

  info("Checking show grid cell options are correct.");
  const { showGridCell } = options;
  const { gridFragmentIndex, rowNumber, columnNumber } = showGridCell;

  is(gridFragmentIndex, "0", "Should be the first grid fragment index.");
  is(rowNumber, "1", "Should be the first grid row.");
  is(columnNumber, "1", "Should be the first grid column.");
});
