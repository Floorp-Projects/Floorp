/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that we can show census group individuals, and then go back to
// the previous view.

"use strict";

const {
  individualsState,
  viewState,
  censusState,
} = require("devtools/client/memory/constants");
const { changeViewAndRefresh, changeView } = require("devtools/client/memory/actions/view");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
  const heapWorker = panel.panelWin.gHeapAnalysesClient;
  const front = panel.panelWin.gFront;
  const store = panel.panelWin.gStore;
  const { getState, dispatch } = store;
  const doc = panel.panelWin.document;

  dispatch(changeView(viewState.CENSUS));

  // Take a snapshot and wait for the census to finish.

  const takeSnapshotButton = doc.getElementById("take-snapshot");
  EventUtils.synthesizeMouseAtCenter(takeSnapshotButton, {}, panel.panelWin);

  yield waitUntilState(store, state => {
    return state.snapshots.length === 1 &&
           state.snapshots[0].census &&
           state.snapshots[0].census.state === censusState.SAVED;
  });

  // Click on the first individuals button found, and wait for the individuals
  // to be fetched.

  const individualsButton = doc.querySelector(".individuals-button");
  EventUtils.synthesizeMouseAtCenter(individualsButton, {}, panel.panelWin);

  yield waitUntilState(store, state => {
    return state.view.state === viewState.INDIVIDUALS &&
           state.individuals &&
           state.individuals.state === individualsState.FETCHED;
  });

  ok(doc.getElementById("shortest-paths"),
     "Should be showing the shortest paths component");
  ok(doc.querySelector(".heap-tree-item"),
     "Should be showing the individuals");

  // Go back to the previous view.

  const popViewButton = doc.getElementById("pop-view-button");
  ok(popViewButton, "Should be showing the #pop-view-button");
  EventUtils.synthesizeMouseAtCenter(popViewButton, {}, panel.panelWin);

  yield waitUntilState(store, state => {
    return state.view.state === viewState.CENSUS;
  });

  ok(!doc.getElementById("shortest-paths"),
     "Should not be showing the shortest paths component anymore");
});
