/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test showing individual objects that do not have allocation stacks.

const {
  censusState,
  viewState,
  individualsState,
  censusDisplays,
} = require("resource://devtools/client/memory/constants.js");
const {
  fetchIndividuals,
  takeSnapshotAndCensus,
} = require("resource://devtools/client/memory/actions/snapshot.js");
const {
  changeView,
} = require("resource://devtools/client/memory/actions/view.js");
const {
  setCensusDisplay,
} = require("resource://devtools/client/memory/actions/census-display.js");

const EXPECTED_INDIVIDUAL_STATES = [
  individualsState.COMPUTING_DOMINATOR_TREE,
  individualsState.FETCHING,
  individualsState.FETCHED,
];

add_task(async function () {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  dispatch(changeView(viewState.CENSUS));
  dispatch(setCensusDisplay(censusDisplays.invertedAllocationStack));

  // Take a snapshot and wait for the census to finish.

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  await waitUntilCensusState(store, s => s.census, [censusState.SAVED]);

  // Fetch individuals.

  const root = getState().snapshots[0].census.report;
  ok(root, "Should have a census");

  const reportLeafIndex = findReportLeafIndex(root, "noStack");
  ok(reportLeafIndex, "Should get a reportLeafIndex for noStack");

  const snapshotId = getState().snapshots[0].id;
  ok(snapshotId, "Should have a snapshot id");

  const breakdown = getState().censusDisplay.breakdown;
  ok(breakdown, "Should have a breakdown");

  dispatch(
    fetchIndividuals(heapWorker, snapshotId, breakdown, reportLeafIndex)
  );

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
    !!getState().individuals.nodes.length,
    "Should have a positive number of nodes"
  );

  heapWorker.destroy();
  await front.detach();
});
