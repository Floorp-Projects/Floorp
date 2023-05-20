/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test clearSnapshots deletes snapshots with state ERROR

const {
  takeSnapshotAndCensus,
  clearSnapshots,
} = require("resource://devtools/client/memory/actions/snapshot.js");
const {
  snapshotState: states,
  treeMapState,
  actions,
} = require("resource://devtools/client/memory/constants.js");

add_task(async function () {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  ok(true, "create a snapshot with a treeMap");
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  await waitUntilSnapshotState(store, [states.SAVED]);
  ok(true, "snapshot created with a SAVED state");
  await waitUntilCensusState(store, snapshot => snapshot.treeMap, [
    treeMapState.SAVED,
  ]);
  ok(true, "treeMap created with a SAVED state");

  ok(true, "set snapshot state to error");
  const id = getState().snapshots[0].id;
  dispatch({ type: actions.SNAPSHOT_ERROR, id, error: new Error("_") });
  await waitUntilSnapshotState(store, [states.ERROR]);
  ok(true, "snapshot set to error state");

  ok(true, "dispatch clearSnapshots action");
  const deleteEvents = Promise.all([
    waitForDispatch(store, actions.DELETE_SNAPSHOTS_START),
    waitForDispatch(store, actions.DELETE_SNAPSHOTS_END),
  ]);
  dispatch(clearSnapshots(heapWorker));
  await deleteEvents;
  ok(true, "received delete snapshots events");
  equal(getState().snapshots.length, 0, "error snapshot deleted");

  heapWorker.destroy();
  await front.detach();
});
