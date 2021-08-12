/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid highlighter is re-displayed after reloading a page and the grid
// item is highlighted.

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

const OTHER_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
    <div id="cell3">cell3</div>
  </div>
`;

add_task(async function() {
  // Disable bfcache for Fission for now.
  // If Fission is disabled, the pref is no-op.
  await SpecialPowers.pushPrefEnv({
    set: [["fission.bfcacheInParent", false]],
  });

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { gridInspector, inspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.GRID;
  const {
    waitForHighlighterTypeRestored,
    waitForHighlighterTypeDiscarded,
  } = getHighlighterTestHelpers(inspector);

  // Don't track reflows since this might cause intermittent failures.
  inspector.off("reflow-in-selected-target", gridInspector.onReflow);

  await selectNode("#grid", inspector);
  const gridList = doc.getElementById("grid-list");
  const checkbox = gridList.children[0].querySelector("input");

  info("Toggling ON the CSS grid highlighter from the layout panel.");
  const onHighlighterShown = highlighters.once("grid-highlighter-shown");
  const onCheckboxChange = waitUntilState(
    store,
    state => state.grids.length == 1 && state.grids[0].highlighted
  );
  checkbox.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info(
    "Check that the CSS grid highlighter is created and the saved grid state."
  );
  is(highlighters.gridHighlighters.size, 1, "CSS grid highlighter is shown.");
  is(
    highlighters.state.grids.size,
    1,
    "There's a saved grid state to be restored."
  );

  info(
    "Reload the page, expect the highlighter to be displayed once again and " +
      "grid is checked"
  );
  const onRestored = waitForHighlighterTypeRestored(HIGHLIGHTER_TYPE);
  let onGridListRestored = waitUntilState(
    store,
    state => state.grids.length == 1 && state.grids[0].highlighted
  );

  const onReloaded = inspector.once("reloaded");
  await refreshTab();
  info("Wait for inspector to be reloaded after page reload");
  await onReloaded;

  await onRestored;
  await onGridListRestored;

  info(
    "Check that the grid highlighter can be displayed after reloading the page"
  );
  is(highlighters.gridHighlighters.size, 1, "CSS grid highlighter is shown.");
  is(
    highlighters.state.grids.size,
    1,
    "The saved grid state has the correct number of saved states."
  );

  info(
    "Navigate to another URL, and check that the highlighter is hidden and " +
      "grid is unchecked"
  );
  const otherUri =
    "data:text/html;charset=utf-8," + encodeURIComponent(OTHER_URI);
  const onDiscarded = waitForHighlighterTypeDiscarded(HIGHLIGHTER_TYPE);
  onGridListRestored = waitUntilState(
    store,
    state => state.grids.length == 1 && !state.grids[0].highlighted
  );
  await navigateTo(otherUri);
  await onDiscarded;
  await onGridListRestored;

  info(
    "Check that the grid highlighter is hidden after navigating to a different page"
  );
  ok(!highlighters.gridHighlighters.size, "CSS grid highlighter is hidden.");
  ok(!highlighters.state.grids.size, "No grids to be restored on page reload.");
});
