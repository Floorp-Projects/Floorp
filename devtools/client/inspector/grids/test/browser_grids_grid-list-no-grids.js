/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that no grid list items and a "no grids available" message is displayed when
// there are no grid containers on the page.

const TEST_URI = `
  <style type='text/css'>
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters } = inspector;

  await selectNode("#grid", inspector);
  const noGridList = doc.querySelector(".grid-pane .devtools-sidepanel-no-result");
  const gridList = doc.getElementById("grid-list");

  info("Checking the initial state of the Grid Inspector.");
  ok(noGridList, "The message no grid containers is displayed.");
  ok(!gridList, "No grid containers are listed.");
  ok(!highlighters.highlighters[HIGHLIGHTER_TYPE],
    "No CSS grid highlighter exists in the highlighters overlay.");
  ok(!highlighters.gridHighlighterShown, "No CSS grid highlighter is shown.");
});
