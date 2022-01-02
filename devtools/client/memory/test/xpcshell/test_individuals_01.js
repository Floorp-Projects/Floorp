/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Basic test for switching to the individuals view.

const {
  censusState,
  viewState,
  individualsState,
} = require("devtools/client/memory/constants");
const {
  fetchIndividuals,
  takeSnapshotAndCensus,
} = require("devtools/client/memory/actions/snapshot");
const { changeView } = require("devtools/client/memory/actions/view");

const EXPECTED_INDIVIDUAL_STATES = [
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

  equal(getState().individuals, null, "no individuals state by default");

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

  dispatch(
    fetchIndividuals(heapWorker, snapshotId, breakdown, reportLeafIndex)
  );

  // Wait for each expected state.
  for (const state of EXPECTED_INDIVIDUAL_STATES) {
    await waitUntilState(store, s => {
      return (
        s.view.state === viewState.INDIVIDUALS &&
        s.individuals &&
        s.individuals.state === state
      );
    });
    ok(true, `Reached state = ${state}`);
  }

  ok(getState().individuals, "Should have individuals state");
  ok(getState().individuals.nodes, "Should have individuals nodes");
  ok(
    getState().individuals.nodes.length > 0,
    "Should have a positive number of nodes"
  );

  heapWorker.destroy();
  await front.detach();
});
