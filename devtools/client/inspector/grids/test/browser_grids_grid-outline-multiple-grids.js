/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid outline is not shown when more than one grid is highlighted.

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

add_task(async function () {
  await pushPref("devtools.gridinspector.maxHighlighters", 2);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;

  await selectNode("#grid1", inspector);
  const gridList = doc.getElementById("grid-list");
  const checkbox1 = gridList.children[0].querySelector("input");
  const checkbox2 = gridList.children[1].querySelector("input");

  info("Toggling ON the CSS grid highlighter for #grid1.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  const onGridOutlineRendered = waitForDOM(doc, "#grid-cell-group rect", 2);
  let onCheckboxChange = waitUntilState(
    store,
    state =>
      state.grids.length === 2 &&
      state.grids[0].highlighted &&
      !state.grids[1].highlighted
  );
  checkbox1.click();
  await onHighlighterShown;
  await onCheckboxChange;
  const elements = await onGridOutlineRendered;

  info("Checking the grid outline for #grid1 is shown.");
  ok(
    doc.getElementById("grid-outline-container"),
    "Grid outline container is rendered."
  );
  is(elements.length, 2, "Grid outline is shown.");

  info("Toggling ON the CSS grid highlighter for #grid2.");
  onHighlighterShown = highlighters.once("grid-highlighter-shown");
  onCheckboxChange = waitUntilState(
    store,
    state =>
      state.grids.length === 2 &&
      state.grids[0].highlighted &&
      state.grids[1].highlighted
  );
  checkbox2.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info("Checking the grid outline is not shown.");
  ok(
    !doc.getElementById("grid-outline-container"),
    "Grid outline is not rendered."
  );
});
