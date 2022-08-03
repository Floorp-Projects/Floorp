/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test clearSnapshots deletes several snapshots

const {
  takeSnapshotAndCensus,
  clearSnapshots,
} = require("devtools/client/memory/actions/snapshot");
const { actions, treeMapState } = require("devtools/client/memory/constants");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  ok(true, "create 2 snapshots with a saved census");
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  ok(true, "snapshots created with a saved census");
  await waitUntilCensusState(store, snapshot => snapshot.treeMap, [
    treeMapState.SAVED,
    treeMapState.SAVED,
  ]);

  const errorHeapWorker = {
    deleteHeapSnapshot() {
      return Promise.reject("_");
    },
  };

  ok(true, "dispatch clearSnapshots action");
  const deleteEvents = Promise.all([
    waitForDispatch(store, actions.DELETE_SNAPSHOTS_START),
    waitForDispatch(store, actions.DELETE_SNAPSHOTS_END),
    waitForDispatch(store, actions.SNAPSHOT_ERROR),
    waitForDispatch(store, actions.SNAPSHOT_ERROR),
  ]);
  dispatch(clearSnapshots(errorHeapWorker));
  await deleteEvents;
  ok(true, "received delete snapshots and snapshot error events");
  equal(getState().snapshots.length, 0, "no snapshot remaining");

  heapWorker.destroy();
  await front.detach();
});
