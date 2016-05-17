/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test switching to the individuals view when we are in the middle of computing
// a dominator tree.

const {
  censusState,
  dominatorTreeState,
  viewState,
  individualsState,
} = require("devtools/client/memory/constants");
const {
  fetchIndividuals,
  takeSnapshotAndCensus,
  computeDominatorTree,
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

  // Start computing a dominator tree.

  dispatch(computeDominatorTree(heapWorker, snapshotId));
  equal(getState().snapshots[0].dominatorTree.state,
        dominatorTreeState.COMPUTING,
        "Should be computing dominator tree");

  // Fetch individuals in the middle of computing the dominator tree.

  dispatch(fetchIndividuals(heapWorker, snapshotId, breakdown,
                            reportLeafIndex));

  // Wait for each expected state.
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
