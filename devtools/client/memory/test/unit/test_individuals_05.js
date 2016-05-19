/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test showing individual objects that do not have allocation stacks.

const {
  censusState,
  viewState,
  individualsState,
  censusDisplays,
} = require("devtools/client/memory/constants");
const {
  fetchIndividuals,
  takeSnapshotAndCensus,
} = require("devtools/client/memory/actions/snapshot");
const {
  changeView,
} = require("devtools/client/memory/actions/view");
const {
  setCensusDisplay,
} = require("devtools/client/memory/actions/census-display");

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
  dispatch(setCensusDisplay(censusDisplays.invertedAllocationStack));

  // Take a snapshot and wait for the census to finish.

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilCensusState(store, s => s.census, [censusState.SAVED]);

  // Fetch individuals.

  const root = getState().snapshots[0].census.report;
  ok(root, "Should have a census");

  const reportLeafIndex = findReportLeafIndex(root, "noStack");
  ok(reportLeafIndex, "Should get a reportLeafIndex for noStack");

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

  heapWorker.destroy();
  yield front.detach();
});
