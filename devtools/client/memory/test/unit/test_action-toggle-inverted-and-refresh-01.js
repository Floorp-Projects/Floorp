/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that changing displays with different inverted state properly
// refreshes the selected census.

const {
  censusDisplays,
  snapshotState: states,
} = require("devtools/client/memory/constants");
const {
  setCensusDisplayAndRefresh
} = require("devtools/client/memory/actions/census-display");
const {
  takeSnapshotAndCensus,
  selectSnapshotAndRefresh,
} = require("devtools/client/memory/actions/snapshot");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  let { getState, dispatch } = store;

  // Select a non-inverted display.
  dispatch(setCensusDisplayAndRefresh(heapWorker, censusDisplays.allocationStack));
  equal(getState().censusDisplay.inverted, false, "not inverted by default");

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));

  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS,
                                       states.SAVED_CENSUS,
                                       states.SAVED_CENSUS]);
  ok(true, "saved 3 snapshots and took a census of each of them");

  // Select an inverted display.
  dispatch(setCensusDisplayAndRefresh(heapWorker, censusDisplays.invertedAllocationStack));

  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS,
                                       states.SAVED_CENSUS,
                                       states.SAVING_CENSUS]);
  ok(true, "toggling inverted should recompute the selected snapshot's census");

  equal(getState().censusDisplay.inverted, true, "now inverted");

  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS,
                                       states.SAVED_CENSUS,
                                       states.SAVED_CENSUS]);

  equal(getState().snapshots[0].census.display.inverted, false);
  equal(getState().snapshots[1].census.display.inverted, false);
  equal(getState().snapshots[2].census.display.inverted, true);

  dispatch(selectSnapshotAndRefresh(heapWorker, getState().snapshots[1].id));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS,
                                       states.SAVING_CENSUS,
                                       states.SAVED_CENSUS]);
  ok(true, "selecting non-inverted census should trigger a recompute");

  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS,
                                       states.SAVED_CENSUS,
                                       states.SAVED_CENSUS]);

  equal(getState().snapshots[0].census.display.inverted, false);
  equal(getState().snapshots[1].census.display.inverted, true);
  equal(getState().snapshots[2].census.display.inverted, true);

  heapWorker.destroy();
  yield front.detach();
});
