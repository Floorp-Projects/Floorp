/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we compute census diffs.

const {
  diffingState,
  snapshotState,
  viewState,
} = require("devtools/client/memory/constants");
const {
  toggleDiffing,
  selectSnapshotForDiffingAndRefresh,
} = require("devtools/client/memory/actions/diffing");
const {
  takeSnapshot,
  readSnapshot,
} = require("devtools/client/memory/actions/snapshot");
const { changeView } = require("devtools/client/memory/actions/view");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;
  dispatch(changeView(viewState.CENSUS));

  equal(getState().diffing, null, "not diffing by default");

  const s1 = await dispatch(takeSnapshot(front, heapWorker));
  const s2 = await dispatch(takeSnapshot(front, heapWorker));
  const s3 = await dispatch(takeSnapshot(front, heapWorker));
  dispatch(readSnapshot(heapWorker, s1));
  dispatch(readSnapshot(heapWorker, s2));
  dispatch(readSnapshot(heapWorker, s3));
  await waitUntilSnapshotState(store, [
    snapshotState.READ,
    snapshotState.READ,
    snapshotState.READ,
  ]);

  dispatch(toggleDiffing());
  dispatch(
    selectSnapshotForDiffingAndRefresh(heapWorker, getState().snapshots[0])
  );
  dispatch(
    selectSnapshotForDiffingAndRefresh(heapWorker, getState().snapshots[1])
  );

  ok(getState().diffing, "We should be diffing.");
  equal(
    getState().diffing.firstSnapshotId,
    getState().snapshots[0].id,
    "First snapshot selected."
  );
  equal(
    getState().diffing.secondSnapshotId,
    getState().snapshots[1].id,
    "Second snapshot selected."
  );

  await waitUntilState(
    store,
    state => state.diffing.state === diffingState.TAKING_DIFF
  );
  ok(
    true,
    "Selecting two snapshots for diffing should trigger computing a diff."
  );

  await waitUntilState(
    store,
    state => state.diffing.state === diffingState.TOOK_DIFF
  );
  ok(true, "And then the diff should complete.");
  ok(getState().diffing.census, "And we should have a census.");
  ok(getState().diffing.census.report, "And that census should have a report.");
  equal(
    getState().diffing.census.display,
    getState().censusDisplay,
    "And that census should have the correct display"
  );
  equal(
    getState().diffing.census.filter,
    getState().filter,
    "And that census should have the correct filter"
  );
  equal(
    getState().diffing.census.display.inverted,
    getState().censusDisplay.inverted,
    "And that census should have the correct inversion"
  );

  heapWorker.destroy();
  await front.detach();
});
