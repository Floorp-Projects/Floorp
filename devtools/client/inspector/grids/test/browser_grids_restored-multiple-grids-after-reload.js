/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid highlighters are re-displayed after reloading a page and multiple
// grids are highlighted.

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
  // Disable bfcache for Fission for now.
  // If Fission is disabled, the pref is no-op.
  await SpecialPowers.pushPrefEnv({
    set: [["fission.bfcacheInParent", false]],
  });

  await pushPref("devtools.gridinspector.maxHighlighters", 3);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { gridInspector, inspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { highlighters, store } = inspector;

  // Don't track reflows since this might cause intermittent failures.
  inspector.off("reflow-in-selected-target", gridInspector.onReflow);

  await selectNode("#grid1", inspector);
  const gridList = doc.getElementById("grid-list");
  const checkbox1 = gridList.children[0].querySelector("input");
  const checkbox2 = gridList.children[1].querySelector("input");
  const checkbox3 = gridList.children[2].querySelector("input");

  info("Toggling ON the CSS grid highlighter for #grid1.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  let onCheckboxChange = waitUntilState(
    store,
    state =>
      state.grids.length == 4 &&
      state.grids[0].highlighted &&
      !state.grids[0].disabled &&
      !state.grids[1].highlighted &&
      !state.grids[1].disabled &&
      !state.grids[2].highlighted &&
      !state.grids[2].disabled &&
      !state.grids[3].highlighted &&
      !state.grids[3].disabled
  );
  checkbox1.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info("Toggling ON the CSS grid highlighter for #grid2.");
  onHighlighterShown = highlighters.once("grid-highlighter-shown");
  onCheckboxChange = waitUntilState(
    store,
    state =>
      state.grids.length == 4 &&
      state.grids[0].highlighted &&
      !state.grids[0].disabled &&
      state.grids[1].highlighted &&
      !state.grids[1].disabled &&
      !state.grids[2].highlighted &&
      !state.grids[2].disabled &&
      !state.grids[3].highlighted &&
      !state.grids[3].disabled
  );
  checkbox2.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info("Toggling ON the CSS grid highlighter for #grid3.");
  onHighlighterShown = highlighters.once("grid-highlighter-shown");
  onCheckboxChange = waitUntilState(
    store,
    state =>
      state.grids.length == 4 &&
      state.grids[0].highlighted &&
      !state.grids[0].disabled &&
      state.grids[1].highlighted &&
      !state.grids[1].disabled &&
      state.grids[2].highlighted &&
      !state.grids[2].disabled &&
      !state.grids[3].highlighted &&
      state.grids[3].disabled
  );
  checkbox3.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info(
    "Check that the CSS grid highlighters are created and the saved grid state."
  );
  is(
    highlighters.gridHighlighters.size,
    3,
    "Got expected number of grid highlighters shown."
  );
  is(
    highlighters.state.grids.size,
    3,
    "Got expected number of grids in the saved state"
  );

  info(
    "Reload the page, expect the highlighters to be displayed once again and " +
      "grids are checked"
  );
  const onStateRestored = waitForNEvents(
    highlighters,
    "highlighter-restored",
    3
  );
  const onGridListRestored = waitUntilState(
    store,
    state =>
      state.grids.length == 4 &&
      state.grids[0].highlighted &&
      !state.grids[0].disabled &&
      state.grids[1].highlighted &&
      !state.grids[1].disabled &&
      state.grids[2].highlighted &&
      !state.grids[2].disabled &&
      !state.grids[3].highlighted &&
      state.grids[3].disabled
  );
  await refreshTab();
  await onStateRestored;
  await onGridListRestored;

  info(
    "Check that the grid highlighters can be displayed after reloading the page"
  );
  is(
    highlighters.gridHighlighters.size,
    3,
    "Got expected number of grid highlighters shown."
  );
  is(
    highlighters.state.grids.size,
    3,
    "Got expected number of grids in the saved state"
  );
});
