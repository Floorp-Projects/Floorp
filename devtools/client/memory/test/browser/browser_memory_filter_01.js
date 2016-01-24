/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that we can show allocation stack breakdowns in the tree.

"use strict";

const {
  dominatorTreeState,
  snapshotState,
  viewState,
} = require("devtools/client/memory/constants");
const { changeViewAndRefresh } = require("devtools/client/memory/actions/view");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
  const heapWorker = panel.panelWin.gHeapAnalysesClient;
  const front = panel.panelWin.gFront;
  const store = panel.panelWin.gStore;
  const { getState, dispatch } = store;
  const doc = panel.panelWin.document;

  ok(!getState().inverted, "not inverted by default");
  const invertCheckbox = doc.getElementById("invert-tree-checkbox");
  EventUtils.synthesizeMouseAtCenter(invertCheckbox, {}, panel.panelWin);
  yield waitUntilState(store, state => state.inverted === true);

  const takeSnapshotButton = doc.getElementById("take-snapshot");
  EventUtils.synthesizeMouseAtCenter(takeSnapshotButton, {}, panel.panelWin);
  yield waitUntilSnapshotState(store, [snapshotState.SAVED_CENSUS]);

  let filterInput = doc.getElementById("filter");
  EventUtils.synthesizeMouseAtCenter(filterInput, {}, panel.panelWin);
  EventUtils.sendString("js::Shape", panel.panelWin);

  yield waitUntilSnapshotState(store, [snapshotState.SAVING_CENSUS]);
  ok(true, "adding a filter string should trigger census recompute");

  yield waitUntilSnapshotState(store, [snapshotState.SAVED_CENSUS]);

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

  yield waitUntilSnapshotState(store, [snapshotState.SAVED_CENSUS]);

  nameElem = doc.querySelector(".heap-tree-item-field.heap-tree-item-name");
  filterInput = doc.getElementById("filter");

  ok(nameElem, "Should still get a tree item row with a name");
  is(nameElem.textContent.trim(), "js::Shape",
    "the tree item should still be the one we filtered for");
  is(filterInput.value, "js::Shape",
    "and filter input still contains the user value");
});
