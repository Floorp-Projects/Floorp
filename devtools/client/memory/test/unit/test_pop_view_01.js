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

const TEST_STATES = [
  individualsState.COMPUTING_DOMINATOR_TREE,
  individualsState.FETCHING,
  individualsState.FETCHED,
];

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  equal(getState().individuals, null,
        "no individuals state by default");

  dispatch(changeView(viewState.CENSUS));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  await waitUntilCensusState(store, s => s.census, [censusState.SAVED]);

  const root = getState().snapshots[0].census.report;
  ok(root, "Should have a census");

  const reportLeafIndex = findReportLeafIndex(root);
  ok(reportLeafIndex, "Should get a reportLeafIndex");

  const snapshotId = getState().snapshots[0].id;
  ok(snapshotId, "Should have a snapshot id");

  const breakdown = getState().snapshots[0].census.display.breakdown;
  ok(breakdown, "Should have a breakdown");

  for (const state of TEST_STATES) {
    dumpn(`Testing popping back to the old view from state = ${state}`);

    dispatch(fetchIndividuals(heapWorker, snapshotId, breakdown,
                              reportLeafIndex));

    // Wait for the expected test state.
    await waitUntilState(store, s => {
      return s.view.state === viewState.INDIVIDUALS &&
             s.individuals &&
             s.individuals.state === state;
    });
    ok(true, `Reached state = ${state}`);

    // Pop back to the CENSUS state.
    dispatch(popViewAndRefresh(heapWorker));
    await waitUntilState(store, s => {
      return s.view.state === viewState.CENSUS;
    });
    ok(!getState().individuals, "Should no longer have individuals");
  }

  heapWorker.destroy();
  await front.detach();
});
