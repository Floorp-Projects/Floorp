/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changing filter state properly refreshes the selected census.

const { viewState, censusState } = require("devtools/client/memory/constants");
const { setFilterStringAndRefresh } = require("devtools/client/memory/actions/filter");
const { takeSnapshotAndCensus, selectSnapshotAndRefresh } = require("devtools/client/memory/actions/snapshot");
const { changeView } = require("devtools/client/memory/actions/view");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  dispatch(changeView(viewState.CENSUS));

  equal(getState().filter, null, "no filter by default");

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));

  await waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED, censusState.SAVED, censusState.SAVED]);
  ok(true, "saved 3 snapshots and took a census of each of them");

  dispatch(setFilterStringAndRefresh("str", heapWorker));
  await waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED, censusState.SAVED, censusState.SAVING]);
  ok(true, "setting filter string should recompute the selected snapshot's census");

  equal(getState().filter, "str", "now inverted");

  await waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED, censusState.SAVED, censusState.SAVED]);

  equal(getState().snapshots[0].census.filter, null);
  equal(getState().snapshots[1].census.filter, null);
  equal(getState().snapshots[2].census.filter, "str");

  dispatch(selectSnapshotAndRefresh(heapWorker, getState().snapshots[1].id));
  await waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED, censusState.SAVING, censusState.SAVED]);
  ok(true, "selecting non-inverted census should trigger a recompute");

  await waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED, censusState.SAVED, censusState.SAVED]);

  equal(getState().snapshots[0].census.filter, null);
  equal(getState().snapshots[1].census.filter, "str");
  equal(getState().snapshots[2].census.filter, "str");

  heapWorker.destroy();
  await front.detach();
});
