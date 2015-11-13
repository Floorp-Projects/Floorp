/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that changing inverted state in the middle of taking a snapshot results
// in an inverted census.

let { snapshotState: states } = require("devtools/client/memory/constants");
let { toggleInverted, toggleInvertedAndRefresh } = require("devtools/client/memory/actions/inverted");
let { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  let { getState, dispatch } = store;

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVING]);

  dispatch(toggleInverted());

  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS]);
  ok(getState().inverted,
     "should want inverted trees");
  ok(getState().snapshots[0].census.inverted,
     "snapshot-we-were-in-the-middle-of-saving's census should be inverted");

  dispatch(toggleInvertedAndRefresh(heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVING_CENSUS]);
  ok(true, "toggling inverted retriggers census");
  ok(!getState().inverted, "no long inverted");

  dispatch(toggleInverted());
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS]);
  ok(getState().inverted, "inverted again");
  ok(getState().snapshots[0].census.inverted,
     "census-we-were-in-the-middle-of-recomputing should be inverted again");

  heapWorker.destroy();
  yield front.detach();
});
