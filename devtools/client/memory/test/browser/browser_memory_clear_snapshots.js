/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests taking and then clearing snapshots.
 */

const { treeMapState } = require("devtools/client/memory/constants");
const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

this.test = makeMemoryTest(TEST_URL, async function({ tab, panel }) {
  const { gStore, document } = panel.panelWin;
  const { getState } = gStore;

  let snapshotEls = document.querySelectorAll("#memory-tool-container .list li");
  is(getState().snapshots.length, 0, "Starts with no snapshots in store");
  is(snapshotEls.length, 0, "No snapshots visible");

  info("Take two snapshots");
  takeSnapshot(panel.panelWin);
  takeSnapshot(panel.panelWin);
  takeSnapshot(panel.panelWin);
  await waitUntilState(gStore, state =>
    state.snapshots.length === 3 &&
    state.snapshots[0].treeMap && state.snapshots[1].treeMap &&
    state.snapshots[2].treeMap &&
    state.snapshots[0].treeMap.state === treeMapState.SAVED &&
    state.snapshots[1].treeMap.state === treeMapState.SAVED &&
    state.snapshots[2].treeMap.state === treeMapState.SAVED);

  snapshotEls = document.querySelectorAll("#memory-tool-container .list li");
  is(snapshotEls.length, 3, "Three snapshots visible");
  is(document.querySelectorAll(".selected").length, 1, "One selected snapshot visible");
  ok(snapshotEls[2].classList.contains("selected"), "Third snapshot selected");

  info("Clicking on first snapshot delete button");
  document.querySelectorAll(".delete")[0].click();

  await waitUntilState(gStore, state =>
    state.snapshots.length === 2 &&
    state.snapshots[0].treeMap && state.snapshots[1].treeMap &&
    state.snapshots[0].treeMap.state === treeMapState.SAVED &&
    state.snapshots[1].treeMap.state === treeMapState.SAVED);

  snapshotEls = document.querySelectorAll(".snapshot-list-item");
  is(snapshotEls.length, 2, "Two snapshots visible");
  // Bug 1476289
  ok(!snapshotEls[0].classList.contains("selected"), "First snapshot not selected");
  ok(snapshotEls[1].classList.contains("selected"), "Second snapshot selected");

  info("Click on Clear Snapshots");
  await clearSnapshots(panel.panelWin);
  is(getState().snapshots.length, 0, "No snapshots in store");
  snapshotEls = document.querySelectorAll("#memory-tool-container .list li");
  is(snapshotEls.length, 0, "No snapshot visible");
});
