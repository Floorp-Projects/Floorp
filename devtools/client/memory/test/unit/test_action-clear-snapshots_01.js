/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test clearSnapshots deletes snapshots with READ censuses

const { takeSnapshotAndCensus, clearSnapshots } = require("devtools/client/memory/actions/snapshot");
const { actions } = require("devtools/client/memory/constants");
const { treeMapState } = require("devtools/client/memory/constants");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  await waitUntilCensusState(store, s => s.treeMap, [treeMapState.SAVED]);
  ok(true, "snapshot created");

  ok(true, "dispatch clearSnapshots action");
  const deleteEvents = Promise.all([
    waitUntilAction(store, actions.DELETE_SNAPSHOTS_START),
    waitUntilAction(store, actions.DELETE_SNAPSHOTS_END)
  ]);
  dispatch(clearSnapshots(heapWorker));
  await deleteEvents;
  ok(true, "received delete snapshots events");

  equal(getState().snapshots.length, 0, "no snapshot remaining");

  heapWorker.destroy();
  await front.detach();
});
