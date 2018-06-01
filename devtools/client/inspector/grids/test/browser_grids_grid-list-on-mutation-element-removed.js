/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid item is removed from the grid list when the grid container is
// removed from the page.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, gridInspector, testActor } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;

  await selectNode("#grid", inspector);
  const gridList = doc.getElementById("grid-list");
  const checkbox = gridList.children[0].querySelector("input");

  info("Checking the initial state of the Grid Inspector.");
  is(gridList.childNodes.length, 1, "One grid container is listed.");
  ok(!highlighters.highlighters[HIGHLIGHTER_TYPE],
    "No CSS grid highlighter exists in the highlighters overlay.");
  ok(!highlighters.gridHighlighterShown, "No CSS grid highlighter is shown.");

  info("Toggling ON the CSS grid highlighter from the layout panel.");
  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(store, state =>
    state.grids.length == 1 && state.grids[0].highlighted);
  checkbox.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info("Checking the CSS grid highlighter is created.");
  ok(highlighters.highlighters[HIGHLIGHTER_TYPE],
    "CSS grid highlighter is created in the highlighters overlay.");
  ok(highlighters.gridHighlighterShown, "CSS grid highlighter is shown.");

  info("Removing the #grid container in the content page.");
  const onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  onCheckboxChange = waitUntilState(store, state => state.grids.length == 0);
  testActor.eval(`
    document.getElementById("grid").remove();
  `);
  await onHighlighterHidden;
  await onCheckboxChange;

  info("Checking the CSS grid highlighter is not shown.");
  ok(!highlighters.gridHighlighterShown, "No CSS grid highlighter is shown.");
  const noGridList = doc.querySelector(".grid-pane .devtools-sidepanel-no-result");
  ok(noGridList, "The message no grid containers is displayed.");
});
