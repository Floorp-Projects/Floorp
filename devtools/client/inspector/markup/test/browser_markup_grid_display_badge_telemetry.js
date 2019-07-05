/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the telemetry count is correct when the grid highlighter is activated from
// the markup view.

const TEST_URI = `
  <style type="text/css">
    #grid {
      display: grid;
    }
  </style>
  <div id="grid"></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  startTelemetry();
  const { inspector } = await openLayoutView();
  const { highlighters, store } = inspector;

  await selectNode("#grid", inspector);
  const gridContainer = await getContainerForSelector("#grid", inspector);
  const gridDisplayBadge = gridContainer.elt.querySelector(
    ".inspector-badge.interactive[data-display]"
  );

  info("Toggling ON the CSS grid highlighter from the grid display badge.");
  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  const onCheckboxChange = waitUntilState(
    store,
    state => state.grids.length === 1 && state.grids[0].highlighted
  );
  gridDisplayBadge.click();
  await onHighlighterShown;
  await onCheckboxChange;

  checkResults();
});

function checkResults() {
  checkTelemetry("devtools.markup.gridinspector.opened", "", 1, "scalar");
}
