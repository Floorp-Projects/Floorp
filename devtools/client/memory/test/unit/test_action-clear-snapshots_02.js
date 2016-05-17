/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test clearSnapshots preserves snapshots with state != READ or ERROR

let { takeSnapshotAndCensus, clearSnapshots, takeSnapshot } = require("devtools/client/memory/actions/snapshot");
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

  ok(true, "create a snapshot with a census in SAVED state");
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  ok(true, "create a snapshot in SAVED state");
  dispatch(takeSnapshot(front));
  yield waitUntilSnapshotState(store, [states.SAVED, states.SAVED]);
  yield waitUntilCensusState(store, snapshot => snapshot.treeMap,
                             [treeMapState.SAVED, null]);
  ok(true, "snapshots created with expected states");

  ok(true, "dispatch clearSnapshots action");
  let deleteEvents = Promise.all([
    waitUntilAction(store, actions.DELETE_SNAPSHOTS_START),
    waitUntilAction(store, actions.DELETE_SNAPSHOTS_END)
  ]);
  dispatch(clearSnapshots(heapWorker));
  yield deleteEvents;
  ok(true, "received delete snapshots events");

  equal(getState().snapshots.length, 1, "one snapshot remaining");
  let remainingSnapshot = getState().snapshots[0];
  equal(remainingSnapshot.treeMap, undefined,
    "remaining snapshot doesn't have a treeMap property");
  equal(remainingSnapshot.census, undefined,
    "remaining snapshot doesn't have a census property");

  heapWorker.destroy();
  yield front.detach();
});
