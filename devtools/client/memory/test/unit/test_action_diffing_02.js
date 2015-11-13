/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that toggling diffing unselects all snapshots.

const { snapshotState } = require("devtools/client/memory/constants");
const { toggleDiffing } = require("devtools/client/memory/actions/diffing");
const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  const { getState, dispatch } = store;

  equal(getState().diffing, null, "not diffing by default");

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilSnapshotState(store, [snapshotState.SAVED_CENSUS,
                                       snapshotState.SAVED_CENSUS,
                                       snapshotState.SAVED_CENSUS]);

  ok(getState().snapshots.some(s => s.selected),
     "One of the new snapshots is selected");

  dispatch(toggleDiffing());
  ok(getState().diffing, "now diffing after toggling");

  for (let s of getState().snapshots) {
    ok(!s.selected,
       "No snapshot should be selected after entering diffing mode");
  }

  heapWorker.destroy();
  yield front.detach();
});
