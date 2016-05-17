/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test clearSnapshots deletes snapshots with state ERROR

let { takeSnapshotAndCensus, clearSnapshots } = require("devtools/client/memory/actions/snapshot");
let { snapshotState: states, treeMapState, actions } = require("devtools/client/memory/constants");

function run_test() {
  run_next_test();
}

add_task(function* () {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  const { getState, dispatch } = store;

  ok(true, "create a snapshot with a treeMap");
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVED]);
  ok(true, "snapshot created with a SAVED state");
  yield waitUntilCensusState(store, snapshot => snapshot.treeMap,
                             [treeMapState.SAVED]);
  ok(true, "treeMap created with a SAVED state");

  ok(true, "set snapshot state to error");
  let id = getState().snapshots[0].id;
  dispatch({ type: actions.SNAPSHOT_ERROR, id, error: new Error("_") });
  yield waitUntilSnapshotState(store, [states.ERROR]);
  ok(true, "snapshot set to error state");

  ok(true, "dispatch clearSnapshots action");
  let deleteEvents = Promise.all([
    waitUntilAction(store, actions.DELETE_SNAPSHOTS_START),
    waitUntilAction(store, actions.DELETE_SNAPSHOTS_END)
  ]);
  dispatch(clearSnapshots(heapWorker));
  yield deleteEvents;
  ok(true, "received delete snapshots events");
  equal(getState().snapshots.length, 0, "error snapshot deleted");

  heapWorker.destroy();
  yield front.detach();
});
