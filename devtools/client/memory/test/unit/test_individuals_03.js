/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test switching to the individuals view when we are in the diffing view.

const {
  censusState,
  diffingState,
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
const {
  selectSnapshotForDiffingAndRefresh,
} = require("devtools/client/memory/actions/diffing");

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

  // Take two snapshots and diff them from each other.

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilCensusState(store, s => s.census,
                             [censusState.SAVED, censusState.SAVED]);

  dispatch(changeView(viewState.DIFFING));
  dispatch(selectSnapshotForDiffingAndRefresh(heapWorker, getState().snapshots[0]));
  dispatch(selectSnapshotForDiffingAndRefresh(heapWorker, getState().snapshots[1]));

  yield waitUntilState(store, state => {
    return state.diffing &&
           state.diffing.state === diffingState.TOOK_DIFF;
  });
  ok(getState().diffing.census);

  // Fetch individuals.

  const root = getState().diffing.census.report;
  ok(root, "Should have a census");

  const reportLeafIndex = findReportLeafIndex(root);
  ok(reportLeafIndex, "Should get a reportLeafIndex");

  const snapshotId = getState().diffing.secondSnapshotId;
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

  // Pop the view back to the diffing.

  dispatch(popViewAndRefresh(heapWorker));

  yield waitUntilState(store, state => {
    return state.diffing &&
      state.diffing.state === diffingState.TOOK_DIFF;
  });

  ok(getState().diffing.census.report,
     "We have our census diff again after popping back to the last view");

  heapWorker.destroy();
  yield front.detach();
});
