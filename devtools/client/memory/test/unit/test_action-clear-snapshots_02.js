/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test clearSnapshots preserves snapshots with state != READ or ERROR

const { takeSnapshotAndCensus, clearSnapshots, takeSnapshot } = require("devtools/client/memory/actions/snapshot");
const { snapshotState: states, treeMapState, actions } = require("devtools/client/memory/constants");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  ok(true, "create a snapshot with a census in SAVED state");
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  ok(true, "create a snapshot in SAVED state");
  dispatch(takeSnapshot(front));
  await waitUntilSnapshotState(store, [states.SAVED, states.SAVED]);
  await waitUntilCensusState(store, snapshot => snapshot.treeMap,
                             [treeMapState.SAVED, null]);
  ok(true, "snapshots created with expected states");

  ok(true, "dispatch clearSnapshots action");
  const deleteEvents = Promise.all([
    waitUntilAction(store, actions.DELETE_SNAPSHOTS_START),
    waitUntilAction(store, actions.DELETE_SNAPSHOTS_END)
  ]);
  dispatch(clearSnapshots(heapWorker));
  await deleteEvents;
  ok(true, "received delete snapshots events");

  equal(getState().snapshots.length, 1, "one snapshot remaining");
  const remainingSnapshot = getState().snapshots[0];
  equal(remainingSnapshot.treeMap, undefined,
    "remaining snapshot doesn't have a treeMap property");
  equal(remainingSnapshot.census, undefined,
    "remaining snapshot doesn't have a census property");

  heapWorker.destroy();
  await front.detach();
});
