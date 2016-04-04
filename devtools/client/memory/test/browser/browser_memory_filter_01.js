/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that we can show allocation stack displays in the tree.

"use strict";

const {
  dominatorTreeState,
  snapshotState,
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

  const takeSnapshotButton = doc.getElementById("take-snapshot");
  EventUtils.synthesizeMouseAtCenter(takeSnapshotButton, {}, panel.panelWin);

  yield waitUntilState(store, state =>
    state.snapshots.length === 1 &&
    state.snapshots[0].census &&
    state.snapshots[0].census.state === censusState.SAVING);

  let filterInput = doc.getElementById("filter");
  EventUtils.synthesizeMouseAtCenter(filterInput, {}, panel.panelWin);
  EventUtils.sendString("js::Shape", panel.panelWin);

  yield waitUntilState(store, state =>
    state.snapshots.length === 1 &&
    state.snapshots[0].census &&
    state.snapshots[0].census.state === censusState.SAVING);
  ok(true, "adding a filter string should trigger census recompute");

  yield waitUntilState(store, state =>
    state.snapshots.length === 1 &&
    state.snapshots[0].census &&
    state.snapshots[0].census.state === censusState.SAVED);

  let nameElem = doc.querySelector(".heap-tree-item-field.heap-tree-item-name");
  ok(nameElem, "Should get a tree item row with a name");
  is(nameElem.textContent.trim(), "js::Shape", "the tree item should be the one we filtered for");
  is(filterInput.value, "js::Shape",
    "and filter input contains the user value");

  // Now switch the dominator view, then switch back to census view
  // and check that the filter word is still correctly applied
  dispatch(changeViewAndRefresh(viewState.DOMINATOR_TREE, heapWorker));
  ok(true, "change view to dominator tree");

  // Wait for the dominator tree to be computed and fetched.
  yield waitUntilDominatorTreeState(store, [dominatorTreeState.LOADED]);
  ok(true, "computed and fetched the dominator tree.");

  dispatch(changeViewAndRefresh(viewState.CENSUS, heapWorker));
  ok(true, "change view back to census");

  yield waitUntilState(store, state =>
    state.snapshots.length === 1 &&
    state.snapshots[0].census &&
    state.snapshots[0].census.state === censusState.SAVED);

  nameElem = doc.querySelector(".heap-tree-item-field.heap-tree-item-name");
  filterInput = doc.getElementById("filter");

  ok(nameElem, "Should still get a tree item row with a name");
  is(nameElem.textContent.trim(), "js::Shape",
    "the tree item should still be the one we filtered for");
  is(filterInput.value, "js::Shape",
    "and filter input still contains the user value");
});
