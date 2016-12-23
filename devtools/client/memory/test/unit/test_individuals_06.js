/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that clearing the current individuals' snapshot leaves the individuals
// view.

const {
  censusState,
  viewState,
  individualsState,
} = require("devtools/client/memory/constants");
const {
  fetchIndividuals,
  takeSnapshotAndCensus,
  clearSnapshots,
} = require("devtools/client/memory/actions/snapshot");
const {
  changeView,
} = require("devtools/client/memory/actions/view");

function run_test() {
  run_next_test();
}

const EXPECTED_INDIVIDUAL_STATES = [
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

  dispatch(changeView(viewState.CENSUS));

  // Take a snapshot and wait for the census to finish.

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilCensusState(store, s => s.census, [censusState.SAVED]);

  // Fetch individuals.

  const root = getState().snapshots[0].census.report;
  ok(root, "Should have a census");

  const reportLeafIndex = findReportLeafIndex(root);
  ok(reportLeafIndex, "Should get a reportLeafIndex");

  const snapshotId = getState().snapshots[0].id;
  ok(snapshotId, "Should have a snapshot id");

  const breakdown = getState().censusDisplay.breakdown;
  ok(breakdown, "Should have a breakdown");

  dispatch(fetchIndividuals(heapWorker, snapshotId, breakdown,
                            reportLeafIndex));

  for (let state of EXPECTED_INDIVIDUAL_STATES) {
    yield waitUntilState(store, s => {
      return s.view.state === viewState.INDIVIDUALS &&
             s.individuals &&
             s.individuals.state === state;
    });
    ok(true, `Reached state = ${state}`);
  }

  ok(getState().individuals, "Should have individuals state");
  ok(getState().individuals.nodes, "Should have individuals nodes");
  ok(getState().individuals.nodes.length > 0,
     "Should have a positive number of nodes");

  dispatch(clearSnapshots(heapWorker));

  equal(getState().view.state, viewState.CENSUS,
        "Went back to census view");

  heapWorker.destroy();
  yield front.detach();
});
