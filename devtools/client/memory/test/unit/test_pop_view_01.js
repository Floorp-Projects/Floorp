/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test popping views from each intermediate individuals model state.

const {
  censusState,
  viewState,
  individualsState,
} = require("devtools/client/memory/constants");
const {
  fetchIndividuals,
  takeSnapshotAndCensus,
} = require("devtools/client/memory/actions/snapshot");
const {
  changeView,
  popViewAndRefresh,
} = require("devtools/client/memory/actions/view");

function run_test() {
  run_next_test();
}

const TEST_STATES = [
  individualsState.COMPUTING_DOMINATOR_TREE,
  individualsState.FETCHING,
  individualsState.FETCHED,
];

add_task(function* () {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  const { getState, dispatch } = store;

  equal(getState().individuals, null,
        "no individuals state by default");

  dispatch(changeView(viewState.CENSUS));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilCensusState(store, s => s.census, [censusState.SAVED]);

  const root = getState().snapshots[0].census.report;
  ok(root, "Should have a census");

  const reportLeafIndex = findReportLeafIndex(root);
  ok(reportLeafIndex, "Should get a reportLeafIndex");

  const snapshotId = getState().snapshots[0].id;
  ok(snapshotId, "Should have a snapshot id");

  const breakdown = getState().snapshots[0].census.display.breakdown;
  ok(breakdown, "Should have a breakdown");

  for (let state of TEST_STATES) {
    dumpn(`Testing popping back to the old view from state = ${state}`);

    dispatch(fetchIndividuals(heapWorker, snapshotId, breakdown,
                              reportLeafIndex));

    // Wait for the expected test state.
    yield waitUntilState(store, s => {
      return s.view.state === viewState.INDIVIDUALS &&
             s.individuals &&
             s.individuals.state === state;
    });
    ok(true, `Reached state = ${state}`);

    // Pop back to the CENSUS state.
    dispatch(popViewAndRefresh(heapWorker));
    yield waitUntilState(store, s => {
      return s.view.state === viewState.CENSUS;
    });
    ok(!getState().individuals, "Should no longer have individuals");
  }

  heapWorker.destroy();
  yield front.detach();
});
