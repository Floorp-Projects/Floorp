/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test clearSnapshots preserves snapshots with state != SAVED_CENSUS or ERROR

let { takeSnapshotAndCensus, clearSnapshots, takeSnapshot } = require("devtools/client/memory/actions/snapshot");
let { snapshotState: states, actions } = require("devtools/client/memory/constants");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  const { getState, dispatch } = store;

  ok(true, "create a snapshot in SAVED_CENSUS state");
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  ok(true, "create a snapshot in SAVED state");
  dispatch(takeSnapshot(front));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS, states.SAVED]);
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
  notEqual(remainingSnapshot.state, states.SAVED_CENSUS,
    "remaining snapshot doesn't have the SAVED_CENSUS state");

  heapWorker.destroy();
  yield front.detach();
});
