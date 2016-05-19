/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests taking and then clearing snapshots.
 */

 const { treeMapState } = require("devtools/client/memory/constants");
 const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

 this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
   const { gStore, document } = panel.panelWin;
   const { getState, dispatch } = gStore;

   let snapshotEls = document.querySelectorAll("#memory-tool-container .list li");
   is(getState().snapshots.length, 0, "Starts with no snapshots in store");
   is(snapshotEls.length, 0, "No snapshots visible");

   info("Take two snapshots");
   takeSnapshot(panel.panelWin);
   takeSnapshot(panel.panelWin);
   yield waitUntilState(gStore, state =>
    state.snapshots.length === 2 &&
    state.snapshots[0].treeMap && state.snapshots[1].treeMap &&
    state.snapshots[0].treeMap.state === treeMapState.SAVED &&
    state.snapshots[1].treeMap.state === treeMapState.SAVED);

   snapshotEls = document.querySelectorAll("#memory-tool-container .list li");
   is(snapshotEls.length, 2, "Two snapshots visible");

   info("Click on Clear Snapshots");
   yield clearSnapshots(panel.panelWin);
   is(getState().snapshots.length, 0, "No snapshots in store");
   snapshotEls = document.querySelectorAll("#memory-tool-container .list li");
   is(snapshotEls.length, 0, "No snapshot visible");
 });
