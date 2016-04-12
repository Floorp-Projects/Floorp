/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test diffing.

"use strict";

const {
  snapshotState,
  diffingState,
  treeMapState
} = require("devtools/client/memory/constants");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
  const heapWorker = panel.panelWin.gHeapAnalysesClient;
  const front = panel.panelWin.gFront;
  const store = panel.panelWin.gStore;
  const { getState, dispatch } = store;
  const doc = panel.panelWin.document;

  ok(!getState().diffing, "Not diffing by default.");

  // Take two snapshots.
  const takeSnapshotButton = doc.getElementById("take-snapshot");
  EventUtils.synthesizeMouseAtCenter(takeSnapshotButton, {}, panel.panelWin);
  yield waitForTime(1000);
  EventUtils.synthesizeMouseAtCenter(takeSnapshotButton, {}, panel.panelWin);

  // Enable diffing mode.
  const diffButton = doc.getElementById("diff-snapshots");
  EventUtils.synthesizeMouseAtCenter(diffButton, {}, panel.panelWin);
  yield waitUntilState(store,
                       state =>
                         !!state.diffing &&
                         state.diffing.state === diffingState.SELECTING);
  ok(true, "Clicking the diffing button put us into the diffing state.");
  is(getDisplayedSnapshotStatus(doc), "Select the baseline snapshot");

  yield waitUntilState(store, state =>
    state.snapshots.length === 2 &&
    state.snapshots[0].treeMap && state.snapshots[1].treeMap &&
    state.snapshots[0].treeMap.state === treeMapState.SAVED &&
    state.snapshots[1].treeMap.state === treeMapState.SAVED);

  const listItems = [...doc.querySelectorAll(".snapshot-list-item")];
  is(listItems.length, 2, "Should have two snapshot list items");

  // Select the first snapshot.
  EventUtils.synthesizeMouseAtCenter(listItems[0], {}, panel.panelWin);
  yield waitUntilState(store,
                       state =>
                         state.diffing.state === diffingState.SELECTING &&
                         state.diffing.firstSnapshotId);
  is(getDisplayedSnapshotStatus(doc),
     "Select the snapshot to compare to the baseline");

  // Select the second snapshot.
  EventUtils.synthesizeMouseAtCenter(listItems[1], {}, panel.panelWin);
  yield waitUntilState(store,
                       state =>
                         state.diffing.state === diffingState.TAKING_DIFF);
  ok(true, "Selecting two snapshots for diffing triggers computing the diff");

  // .startsWith because the ellipsis is lost in translation.
  ok(getDisplayedSnapshotStatus(doc).startsWith("Computing difference"));

  yield waitUntilState(store,
                       state => state.diffing.state === diffingState.TOOK_DIFF);
  ok(true, "And that diff is computed successfully");
  is(getDisplayedSnapshotStatus(doc), null, "No status text anymore");
  ok(doc.querySelector(".heap-tree-item"), "And instead we should be showing the tree");
});
