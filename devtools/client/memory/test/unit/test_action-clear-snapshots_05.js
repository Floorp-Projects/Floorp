/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test clearSnapshots deletes several snapshots

let { takeSnapshotAndCensus, clearSnapshots } = require("devtools/client/memory/actions/snapshot");
let { snapshotState: states, actions, treeMapState } = require("devtools/client/memory/constants");

function run_test() {
  run_next_test();
}

add_task(function* () {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  const { getState, dispatch } = store;

  ok(true, "create 2 snapshots with a saved census");
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  ok(true, "snapshots created with a saved census");
  yield waitUntilCensusState(store, snapshot => snapshot.treeMap,
                             [treeMapState.SAVED, treeMapState.SAVED]);

  let errorHeapWorker = {
    deleteHeapSnapshot: function () {
      return Promise.reject("_");
    }
  };

  ok(true, "dispatch clearSnapshots action");
  let deleteEvents = Promise.all([
    waitUntilAction(store, actions.DELETE_SNAPSHOTS_START),
    waitUntilAction(store, actions.DELETE_SNAPSHOTS_END),
    waitUntilAction(store, actions.SNAPSHOT_ERROR),
    waitUntilAction(store, actions.SNAPSHOT_ERROR),
  ]);
  dispatch(clearSnapshots(errorHeapWorker));
  yield deleteEvents;
  ok(true, "received delete snapshots and snapshot error events");
  equal(getState().snapshots.length, 0, "no snapshot remaining");

  heapWorker.destroy();
  yield front.detach();
});
