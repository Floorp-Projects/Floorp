/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the multiple grid highlighters in the grid inspector panel.

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
  <div id="grid4" class="grid">
    <div class="cell1">cell1</div>
    <div class="cell2">cell2</div>
  </div>
`;

add_task(async function() {
  await pushPref("devtools.gridinspector.maxHighlighters", 3);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;

  await selectNode("#grid1", inspector);
  const gridList = doc.getElementById("grid-list");
  const checkbox1 = gridList.children[0].querySelector("input");
  const checkbox2 = gridList.children[1].querySelector("input");
  const checkbox3 = gridList.children[2].querySelector("input");
  const checkbox4 = gridList.children[3].querySelector("input");

  info("Checking the initial state of the Grid Inspector.");
  is(gridList.childNodes.length, 4, "4 grid containers are listed.");
  ok(!checkbox1.checked, `Grid item ${checkbox1.value} is unchecked in the grid list.`);
  ok(!checkbox2.checked, `Grid item ${checkbox2.value} is unchecked in the grid list.`);
  ok(!checkbox3.checked, `Grid item ${checkbox3.value} is unchecked in the grid list.`);
  ok(!checkbox4.checked, `Grid item ${checkbox4.value} is unchecked in the grid list.`);
  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");

  info("Toggling ON the CSS grid highlighter for #grid1.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 4 &&
    state.grids[0].highlighted && !state.grids[0].disabled &&
    !state.grids[1].highlighted && !state.grids[1].disabled &&
    !state.grids[2].highlighted && !state.grids[2].disabled &&
    !state.grids[3].highlighted && !state.grids[3].disabled);
  checkbox1.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info("Check that the CSS grid highlighter is created and the saved grid state.");
  is(highlighters.gridHighlighters.size, 1,
    "Got expected number of grid highlighters shown.");
  is(highlighters.state.grids.size, 1, "Got expected number of grids in the saved state");

  info("Toggling ON the CSS grid highlighter for #grid2.");
  onHighlighterShown = highlighters.once("grid-highlighter-shown");
  onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 4 &&
    state.grids[0].highlighted && !state.grids[0].disabled &&
    state.grids[1].highlighted && !state.grids[1].disabled &&
    !state.grids[2].highlighted && !state.grids[2].disabled &&
    !state.grids[3].highlighted && !state.grids[3].disabled);
  checkbox2.click();
  await onHighlighterShown;
  await onCheckboxChange;

  is(highlighters.gridHighlighters.size, 2,
    "Got expected number of grid highlighters shown.");
  is(highlighters.state.grids.size, 2, "Got expected number of grids in the saved state");

  info("Toggling ON the CSS grid highlighter for #grid3.");
  onHighlighterShown = highlighters.once("grid-highlighter-shown");
  onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 4 &&
    state.grids[0].highlighted && !state.grids[0].disabled &&
    state.grids[1].highlighted && !state.grids[1].disabled &&
    state.grids[2].highlighted && !state.grids[2].disabled &&
    !state.grids[3].highlighted && state.grids[3].disabled);
  checkbox3.click();
  await onHighlighterShown;
  await onCheckboxChange;

  is(highlighters.gridHighlighters.size, 3,
    "Got expected number of grid highlighters shown.");
  is(highlighters.state.grids.size, 3, "Got expected number of grids in the saved state");

  info("Toggling OFF the CSS grid highlighter for #grid3.");
  let onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 4 &&
    state.grids[0].highlighted && !state.grids[0].disabled &&
    state.grids[1].highlighted && !state.grids[1].disabled &&
    !state.grids[2].highlighted && !state.grids[2].disabled &&
    !state.grids[3].highlighted && !state.grids[3].disabled);
  checkbox3.click();
  await onHighlighterHidden;
  await onCheckboxChange;

  is(highlighters.gridHighlighters.size, 2,
    "Got expected number of grid highlighters shown.");
  is(highlighters.state.grids.size, 2, "Got expected number of grids in the saved state");

  info("Toggling OFF the CSS grid highlighter for #grid2.");
  onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 4 &&
    state.grids[0].highlighted && !state.grids[0].disabled &&
    !state.grids[1].highlighted && !state.grids[1].disabled &&
    !state.grids[2].highlighted && !state.grids[2].disabled &&
    !state.grids[3].highlighted && !state.grids[3].disabled);
  checkbox2.click();
  await onHighlighterHidden;
  await onCheckboxChange;

  is(highlighters.gridHighlighters.size, 1,
    "Got expected number of grid highlighters shown.");
  is(highlighters.state.grids.size, 1, "Got expected number of grids in the saved state");

  info("Toggling OFF the CSS grid highlighter for #grid1.");
  onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 4 &&
    !state.grids[0].highlighted && !state.grids[0].disabled &&
    !state.grids[1].highlighted && !state.grids[1].disabled &&
    !state.grids[2].highlighted && !state.grids[2].disabled &&
    !state.grids[3].highlighted && !state.grids[3].disabled);
  checkbox1.click();
  await onHighlighterHidden;
  await onCheckboxChange;

  info("Check that the CSS grid highlighter is not shown and the saved grid state.");
  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");
  ok(!highlighters.state.grids.size, "No grids in the saved state");
});
