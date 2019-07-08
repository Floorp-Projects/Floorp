/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the telemetry count is correct when the grid highlighter is activated from
// the layout view.

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
  startTelemetry();
  const { gridInspector, inspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;

  await selectNode("#grid", inspector);
  const gridList = doc.getElementById("grid-list");
  const checkbox = gridList.children[0].querySelector("input");

  info("Toggling ON the CSS grid highlighter from the layout panel.");
  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(
    store,
    state => state.grids.length == 1 && state.grids[0].highlighted
  );
  checkbox.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info("Toggling OFF the CSS grid highlighter from the layout panel.");
  const onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  onCheckboxChange = waitUntilState(
    store,
    state => state.grids.length == 1 && !state.grids[0].highlighted
  );
  checkbox.click();
  await onHighlighterHidden;
  await onCheckboxChange;

  checkResults();
});

function checkResults() {
  checkTelemetry("devtools.grid.gridinspector.opened", "", 1, "scalar");
  checkTelemetry(
    "DEVTOOLS_GRID_HIGHLIGHTER_TIME_ACTIVE_SECONDS",
    "",
    null,
    "hasentries"
  );
}
