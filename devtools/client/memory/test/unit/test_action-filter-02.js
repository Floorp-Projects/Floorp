/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that changing filter state properly refreshes the selected census.

let { snapshotState: states, viewState, censusState } = require("devtools/client/memory/constants");
let { setFilterStringAndRefresh } = require("devtools/client/memory/actions/filter");
let { takeSnapshotAndCensus, selectSnapshotAndRefresh } = require("devtools/client/memory/actions/snapshot");
let { setCensusDisplay } = require("devtools/client/memory/actions/census-display");
let { changeView } = require("devtools/client/memory/actions/view");

function run_test() {
  run_next_test();
}

add_task(function* () {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  let { getState, dispatch } = store;

  dispatch(changeView(viewState.CENSUS));

  equal(getState().filter, null, "no filter by default");

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));

  yield waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED,
                              censusState.SAVED,
                              censusState.SAVED]);
  ok(true, "saved 3 snapshots and took a census of each of them");

  dispatch(setFilterStringAndRefresh("str", heapWorker));
  yield waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED,
                              censusState.SAVED,
                              censusState.SAVING]);
  ok(true, "setting filter string should recompute the selected snapshot's census");

  equal(getState().filter, "str", "now inverted");

  yield waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED,
                              censusState.SAVED,
                              censusState.SAVED]);

  equal(getState().snapshots[0].census.filter, null);
  equal(getState().snapshots[1].census.filter, null);
  equal(getState().snapshots[2].census.filter, "str");

  dispatch(selectSnapshotAndRefresh(heapWorker, getState().snapshots[1].id));
  yield waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED,
                              censusState.SAVING,
                              censusState.SAVED]);
  ok(true, "selecting non-inverted census should trigger a recompute");

  yield waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED,
                              censusState.SAVED,
                              censusState.SAVED]);

  equal(getState().snapshots[0].census.filter, null);
  equal(getState().snapshots[1].census.filter, "str");
  equal(getState().snapshots[2].census.filter, "str");

  heapWorker.destroy();
  yield front.detach();
});
