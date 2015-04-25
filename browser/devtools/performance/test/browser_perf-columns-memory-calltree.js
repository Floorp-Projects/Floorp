/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the memory call tree view renders the correct columns.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, $$, DetailsView, MemoryCallTreeView } = panel.panelWin;

  // Enable memory to test.
  Services.prefs.setBoolPref(MEMORY_PREF, true);

  yield startRecording(panel);
  yield busyWait(1000);

  let rendered = once(MemoryCallTreeView, EVENTS.MEMORY_CALL_TREE_RENDERED);
  yield stopRecording(panel);
  yield DetailsView.selectView("memory-calltree");
  ok(DetailsView.isViewSelected(MemoryCallTreeView), "The call tree is now selected.");
  yield rendered;

  testCells($, $$, {
    "duration": false,
    "percentage": false,
    "allocations": true,
    "self-duration": false,
    "self-percentage": false,
    "self-allocations": true,
    "samples": false,
    "function": true
  });

  yield teardown(panel);
  finish();
}

function testCells($, $$, visibleCells) {
  for (let cell in visibleCells) {
    if (visibleCells[cell]) {
      ok($(`.call-tree-cell[type=${cell}]`),
        `At least one ${cell} column was visible in the tree.`);
    } else {
      ok(!$(`.call-tree-cell[type=${cell}]`),
        `No ${cell} columns were visible in the tree.`);
    }
  }

  is($$(".call-tree-cell", $(".call-tree-item")).length,
    Object.keys(visibleCells).filter(e => visibleCells[e]).length,
    "The correct number of cells were found in the tree.");
}
