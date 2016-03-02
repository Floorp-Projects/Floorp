/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that changing inverted state in the middle of taking a snapshot results
// in an inverted census.

const { censusDisplays, snapshotState: states } = require("devtools/client/memory/constants");
const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
const { setCensusDisplayAndRefresh } = require("devtools/client/memory/actions/census-display");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  let { getState, dispatch } = store;

  dispatch(setCensusDisplayAndRefresh(heapWorker, censusDisplays.allocationStack));
  equal(getState().censusDisplay.inverted, false);

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVING]);

  dispatch(setCensusDisplayAndRefresh(heapWorker, censusDisplays.invertedAllocationStack));

  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS]);
  ok(getState().censusDisplay.inverted,
     "should want inverted trees");
  ok(getState().snapshots[0].census.display.inverted,
     "snapshot-we-were-in-the-middle-of-saving's census should be inverted");

  dispatch(setCensusDisplayAndRefresh(heapWorker, censusDisplays.allocationStack));
  yield waitUntilSnapshotState(store, [states.SAVING_CENSUS]);
  ok(true, "toggling inverted retriggers census");
  ok(!getState().censusDisplay.inverted, "no longer inverted");

  dispatch(setCensusDisplayAndRefresh(heapWorker, censusDisplays.invertedAllocationStack));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS]);
  ok(getState().censusDisplay.inverted, "inverted again");
  ok(getState().snapshots[0].census.display.inverted,
     "census-we-were-in-the-middle-of-recomputing should be inverted again");

  heapWorker.destroy();
  yield front.detach();
});
