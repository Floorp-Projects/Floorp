/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid list updates when a new grid container is added to the page.

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
  <div id="grid2">
    <div class="cell1">cell1</div>
    <div class="cell2">cell2</div>
  </div>
`;

add_task(async function() {
  await pushPref("devtools.gridinspector.maxHighlighters", 1);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;

  await selectNode("#grid", inspector);
  const gridList = doc.getElementById("grid-list");
  const checkbox1 = gridList.children[0].querySelector("input");

  info("Checking the initial state of the Grid Inspector.");
  is(gridList.childNodes.length, 1, "One grid container is listed.");
  ok(
    !highlighters.gridHighlighters.size,
    "No CSS grid highlighter exists in the highlighters overlay."
  );

  info("Toggling ON the CSS grid highlighter from the layout panel.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  checkbox1.click();
  await onHighlighterShown;

  info("Checking the CSS grid highlighter is created.");
  is(highlighters.gridHighlighters.size, 1, "CSS grid highlighter is shown.");

  info("Adding the #grid2 container in the content page.");
  const onGridListUpdate = waitUntilState(
    store,
    state =>
      state.grids.length == 2 &&
      state.grids[0].highlighted &&
      !state.grids[1].highlighted
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () =>
    content.document.getElementById("grid2").classList.add("grid")
  );
  await onGridListUpdate;

  info("Checking the new Grid Inspector state.");
  is(gridList.childNodes.length, 2, "Two grid containers are listed.");
  is(highlighters.gridHighlighters.size, 1, "CSS grid highlighter is shown.");

  const checkbox2 = gridList.children[1].querySelector("input");

  info("Toggling ON the CSS grid highlighter for #grid2.");
  onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(
    store,
    state =>
      state.grids.length == 2 &&
      !state.grids[0].highlighted &&
      state.grids[1].highlighted
  );
  checkbox2.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info("Checking the CSS grid highlighter is still shown.");
  is(highlighters.gridHighlighters.size, 1, "CSS grid highlighter is shown.");

  info("Toggling OFF the CSS grid highlighter from the layout panel.");
  const onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  onCheckboxChange = waitUntilState(
    store,
    state =>
      state.grids.length == 2 &&
      !state.grids[0].highlighted &&
      !state.grids[1].highlighted
  );
  checkbox2.click();
  await onHighlighterHidden;
  await onCheckboxChange;

  info("Checking the CSS grid highlighter is not shown.");
  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");
});
