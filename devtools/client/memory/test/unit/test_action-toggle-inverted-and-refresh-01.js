/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changing displays with different inverted state properly
// refreshes the selected census.

const {
  censusDisplays,
  censusState,
  viewState
} = require("devtools/client/memory/constants");
const {
  setCensusDisplayAndRefresh
} = require("devtools/client/memory/actions/census-display");
const {
  takeSnapshotAndCensus,
  selectSnapshotAndRefresh,
} = require("devtools/client/memory/actions/snapshot");
const { changeView } = require("devtools/client/memory/actions/view");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  dispatch(changeView(viewState.CENSUS));

  // Select a non-inverted display.
  dispatch(setCensusDisplayAndRefresh(heapWorker, censusDisplays.allocationStack));
  equal(getState().censusDisplay.inverted, false, "not inverted by default");

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));

  await waitUntilCensusState(store, s => s.census,
                             [censusState.SAVED, censusState.SAVED, censusState.SAVED]);
  ok(true, "saved 3 snapshots and took a census of each of them");

  // Select an inverted display.
  dispatch(setCensusDisplayAndRefresh(heapWorker,
                                      censusDisplays.invertedAllocationStack));

  await waitUntilCensusState(store, s => s.census,
                             [censusState.SAVED, censusState.SAVED, censusState.SAVING]);
  ok(true, "toggling inverted should recompute the selected snapshot's census");

  equal(getState().censusDisplay.inverted, true, "now inverted");

  await waitUntilCensusState(store, s => s.census,
                             [censusState.SAVED, censusState.SAVED, censusState.SAVED]);

  equal(getState().snapshots[0].census.display.inverted, false);
  equal(getState().snapshots[1].census.display.inverted, false);
  equal(getState().snapshots[2].census.display.inverted, true);

  dispatch(selectSnapshotAndRefresh(heapWorker, getState().snapshots[1].id));
  await waitUntilCensusState(store, s => s.census,
                             [censusState.SAVED, censusState.SAVING, censusState.SAVED]);
  ok(true, "selecting non-inverted census should trigger a recompute");

  await waitUntilCensusState(store, s => s.census,
                             [censusState.SAVED, censusState.SAVED, censusState.SAVED]);

  equal(getState().snapshots[0].census.display.inverted, false);
  equal(getState().snapshots[1].census.display.inverted, true);
  equal(getState().snapshots[2].census.display.inverted, true);

  heapWorker.destroy();
  await front.detach();
});
