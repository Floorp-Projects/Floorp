/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that using ACCEL+UP/DOWN, the user can navigate between snapshots.

"use strict";

const {
  viewState,
  censusState
} = require("devtools/client/memory/constants");
const {
  takeSnapshotAndCensus
} = require("devtools/client/memory/actions/snapshot");
const { changeView } = require("devtools/client/memory/actions/view");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

this.test = makeMemoryTest(TEST_URL, function* ({ panel }) {
  // Creating snapshots already takes ~25 seconds on linux 32 debug machines
  // which makes the test very likely to go over the allowed timeout
  requestLongerTimeout(2);

  const heapWorker = panel.panelWin.gHeapAnalysesClient;
  const front = panel.panelWin.gFront;
  const store = panel.panelWin.gStore;
  const { dispatch } = store;
  const doc = panel.panelWin.document;

  dispatch(changeView(viewState.CENSUS));

  info("Take 3 snapshots");
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));

  yield waitUntilState(store, state =>
    state.snapshots.length == 3 &&
    state.snapshots.every(s => s.census && s.census.state === censusState.SAVED));
  ok(true, "All snapshots censuses are in SAVED state");

  yield waitUntilSnapshotSelected(store, 2);
  ok(true, "Third snapshot selected after creating all snapshots.");

  info("Press ACCEL+UP key, expect second snapshot selected.");
  EventUtils.synthesizeKey("VK_UP", { accelKey: true }, panel.panelWin);
  yield waitUntilSnapshotSelected(store, 1);
  ok(true, "Second snapshot selected after alt+UP.");

  info("Press ACCEL+UP key, expect first snapshot selected.");
  EventUtils.synthesizeKey("VK_UP", { accelKey: true }, panel.panelWin);
  yield waitUntilSnapshotSelected(store, 0);
  ok(true, "First snapshot is selected after ACCEL+UP");

  info("Check ACCEL+UP is a noop when the first snapshot is selected.");
  EventUtils.synthesizeKey("VK_UP", { accelKey: true }, panel.panelWin);
  // We assume the snapshot selection should be synchronous here.
  is(getSelectedSnapshotIndex(store), 0, "First snapshot is still selected");

  info("Press ACCEL+DOWN key, expect second snapshot selected.");
  EventUtils.synthesizeKey("VK_DOWN", { accelKey: true }, panel.panelWin);
  yield waitUntilSnapshotSelected(store, 1);
  ok(true, "Second snapshot is selected after ACCEL+DOWN");

  info("Click on first node.");
  let firstNode = doc.querySelector(".tree .heap-tree-item-name");
  EventUtils.synthesizeMouseAtCenter(firstNode, {}, panel.panelWin);
  yield waitUntilState(store, state => state.snapshots[1].census.focused ===
      state.snapshots[1].census.report.children[0]
  );
  ok(true, "First root is selected after click.");

  info("Press DOWN key, expect second root focused.");
  EventUtils.synthesizeKey("VK_DOWN", {}, panel.panelWin);
  yield waitUntilState(store, state => state.snapshots[1].census.focused ===
      state.snapshots[1].census.report.children[1]
  );
  ok(true, "Second root is selected after pressing DOWN.");
  is(getSelectedSnapshotIndex(store), 1, "Second snapshot is still selected");

  info("Press UP key, expect second root focused.");
  EventUtils.synthesizeKey("VK_UP", {}, panel.panelWin);
  yield waitUntilState(store, state => state.snapshots[1].census.focused ===
      state.snapshots[1].census.report.children[0]
  );
  ok(true, "First root is selected after pressing UP.");
  is(getSelectedSnapshotIndex(store), 1, "Second snapshot is still selected");

  info("Press ACCEL+DOWN key, expect third snapshot selected.");
  EventUtils.synthesizeKey("VK_DOWN", { accelKey: true }, panel.panelWin);
  yield waitUntilSnapshotSelected(store, 2);
  ok(true, "Thirdˆ snapshot is selected after ACCEL+DOWN");

  info("Check ACCEL+DOWN is a noop when the last snapshot is selected.");
  EventUtils.synthesizeKey("VK_DOWN", { accelKey: true }, panel.panelWin);
  // We assume the snapshot selection should be synchronous here.
  is(getSelectedSnapshotIndex(store), 2, "Third snapshot is still selected");
});
