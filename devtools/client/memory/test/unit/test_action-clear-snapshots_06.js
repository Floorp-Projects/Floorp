/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that clearSnapshots disables diffing when deleting snapshots

const {
  takeSnapshotAndCensus,
  clearSnapshots } = require("devtools/client/memory/actions/snapshot");
const {
  snapshotState: states,
  actions } = require("devtools/client/memory/constants");
const {
  toggleDiffing,
  selectSnapshotForDiffingAndRefresh
} = require("devtools/client/memory/actions/diffing");

function run_test() {
  run_next_test();
}

add_task(function* () {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  const { getState, dispatch } = store;

  ok(true, "Create 2 snapshots in SAVED_CENSUS state");
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  ok(true, "Snapshots created in SAVED_CENSUS state");
  yield waitUntilSnapshotState(store,
    [states.SAVED_CENSUS, states.SAVED_CENSUS]);

  dispatch(toggleDiffing());
  dispatch(selectSnapshotForDiffingAndRefresh(heapWorker,
                                              getState().snapshots[0]));
  dispatch(selectSnapshotForDiffingAndRefresh(heapWorker,
                                              getState().snapshots[1]));
  ok(getState().diffing, "We should be in diffing view");

  ok(true, "Dispatch clearSnapshots action");
  let deleteEvents = Promise.all([
    waitUntilAction(store, actions.DELETE_SNAPSHOTS_START),
    waitUntilAction(store, actions.DELETE_SNAPSHOTS_END)
  ]);
  dispatch(clearSnapshots(heapWorker));
  yield deleteEvents;
  ok(true, "received delete snapshots events");

  ok(getState().snapshots.length === 0, "Snapshots array should be empty");
  ok(!getState().diffing, "We should no longer be diffing");

  heapWorker.destroy();
  yield front.detach();
});
