/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the grid highlighter in the grid inspector panel with multiple grids in
// the page.

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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let { inspector, gridInspector } = yield openLayoutView();
  let { document: doc } = gridInspector;
  let { highlighters, store } = inspector;

  yield selectNode("#grid1", inspector);
  let gridList = doc.getElementById("grid-list");
  let checkbox1 = gridList.children[0].querySelector("input");
  let checkbox2 = gridList.children[1].querySelector("input");

  info("Checking the initial state of the Grid Inspector.");
  is(gridList.childNodes.length, 2, "2 grid containers are listed.");
  ok(!checkbox1.checked, `Grid item ${checkbox1.value} is unchecked in the grid list.`);
  ok(!checkbox2.checked, `Grid item ${checkbox2.value} is unchecked in the grid list.`);
  ok(!highlighters.highlighters[HIGHLIGHTER_TYPE],
    "No CSS grid highlighter exists in the highlighters overlay.");
  ok(!highlighters.gridHighlighterShown, "No CSS grid highlighter is shown.");

  info("Toggling ON the CSS grid highlighter for #grid1.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 2 &&
    state.grids[0].highlighted &&
    !state.grids[1].highlighted);
  checkbox1.click();
  yield onHighlighterShown;
  yield onCheckboxChange;

  info("Checking the CSS grid highlighter is created.");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "CSS grid highlighter is created in the highlighters overlay.");
  ok(highlighters.gridHighlighterShown, "CSS grid highlighter is shown.");

  info("Toggling ON the CSS grid highlighter for #grid2.");
  onHighlighterShown = highlighters.once("grid-highlighter-shown");
  onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 2 &&
    !state.grids[0].highlighted &&
    state.grids[1].highlighted);
  checkbox2.click();
  yield onHighlighterShown;
  yield onCheckboxChange;

  info("Checking the CSS grid highlighter is still shown.");
  ok(highlighters.gridHighlighterShown, "CSS grid highlighter is shown.");

  info("Toggling OFF the CSS grid highlighter from the layout panel.");
  let onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 2 &&
    !state.grids[0].highlighted &&
    !state.grids[1].highlighted);
  checkbox2.click();
  yield onHighlighterHidden;
  yield onCheckboxChange;

  info("Checking the CSS grid highlighter is not shown.");
  ok(!highlighters.gridHighlighterShown, "No CSS grid highlighter is shown.");
});
