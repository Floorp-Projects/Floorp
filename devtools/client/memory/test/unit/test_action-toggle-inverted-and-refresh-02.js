/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changing inverted state in the middle of taking a snapshot results
// in an inverted census.

const { censusDisplays, snapshotState: states, censusState, viewState } = require("devtools/client/memory/constants");
const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
const {
  setCensusDisplay,
  setCensusDisplayAndRefresh,
} = require("devtools/client/memory/actions/census-display");
const { changeView } = require("devtools/client/memory/actions/view");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  dispatch(changeView(viewState.CENSUS));

  dispatch(setCensusDisplay(censusDisplays.allocationStack));
  equal(getState().censusDisplay.inverted, false,
        "Should not have an inverted census display");

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  await waitUntilSnapshotState(store, [states.SAVING]);

  dispatch(setCensusDisplayAndRefresh(heapWorker,
                                      censusDisplays.invertedAllocationStack));

  await waitUntilCensusState(store, s => s.census, [censusState.SAVED]);

  ok(getState().censusDisplay.inverted,
     "should want inverted trees");
  ok(getState().snapshots[0].census.display.inverted,
     "snapshot-we-were-in-the-middle-of-saving's census should be inverted");

  dispatch(setCensusDisplayAndRefresh(heapWorker, censusDisplays.allocationStack));
  await waitUntilCensusState(store, s => s.census, [censusState.SAVING]);
  ok(true, "toggling inverted retriggers census");
  ok(!getState().censusDisplay.inverted, "no longer inverted");

  dispatch(setCensusDisplayAndRefresh(heapWorker,
                                      censusDisplays.invertedAllocationStack));
  await waitUntilCensusState(store, s => s.census, [censusState.SAVED]);
  ok(getState().censusDisplay.inverted, "inverted again");
  ok(getState().snapshots[0].census.display.inverted,
     "census-we-were-in-the-middle-of-recomputing should be inverted again");

  heapWorker.destroy();
  await front.detach();
});
