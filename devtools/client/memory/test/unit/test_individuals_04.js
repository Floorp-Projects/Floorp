/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test showing individual Array objects.

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
} = require("devtools/client/memory/actions/view");
const {
  setFilterString,
} = require("devtools/client/memory/actions/filter");

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

  dispatch(changeView(viewState.CENSUS));
  dispatch(setFilterString("Array"));

  // Take a snapshot and wait for the census to finish.

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  await waitUntilCensusState(store, s => s.census, [censusState.SAVED]);

  // Fetch individuals.

  const root = getState().snapshots[0].census.report;
  ok(root, "Should have a census");

  const reportLeafIndex = findReportLeafIndex(root, "Array");
  ok(reportLeafIndex, "Should get a reportLeafIndex for Array");

  const snapshotId = getState().snapshots[0].id;
  ok(snapshotId, "Should have a snapshot id");

  const breakdown = getState().censusDisplay.breakdown;
  ok(breakdown, "Should have a breakdown");

  dispatch(fetchIndividuals(heapWorker, snapshotId, breakdown,
                            reportLeafIndex));

  for (const state of EXPECTED_INDIVIDUAL_STATES) {
    await waitUntilState(store, s => {
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

  // Assert that all the individuals are `Array`s.

  for (const node of getState().individuals.nodes) {
    dumpn("Checking node: " + node.label.join(" > "));
    ok(node.label.find(part => part === "Array"),
       "The node should be an Array node");
  }

  heapWorker.destroy();
  await front.detach();
});
