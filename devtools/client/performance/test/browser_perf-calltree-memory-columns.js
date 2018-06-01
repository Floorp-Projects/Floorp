/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the memory call tree view renders the correct columns.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_ALLOCATIONS_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { EVENTS, $, $$, DetailsView, MemoryCallTreeView } = panel.panelWin;

  // Enable allocations to test.
  Services.prefs.setBoolPref(UI_ENABLE_ALLOCATIONS_PREF, true);

  await startRecording(panel);
  await stopRecording(panel);

  const rendered = once(MemoryCallTreeView, EVENTS.UI_MEMORY_CALL_TREE_RENDERED);
  await DetailsView.selectView("memory-calltree");
  await rendered;

  ok(DetailsView.isViewSelected(MemoryCallTreeView), "The call tree is now selected.");

  testCells($, $$, {
    "duration": false,
    "percentage": false,
    "count": true,
    "count-percentage": true,
    "size": true,
    "size-percentage": true,
    "self-duration": false,
    "self-percentage": false,
    "self-count": true,
    "self-count-percentage": true,
    "self-size": true,
    "self-size-percentage": true,
    "samples": false,
    "function": true
  });

  await teardownToolboxAndRemoveTab(panel);
});

function testCells($, $$, visibleCells) {
  for (const cell in visibleCells) {
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
