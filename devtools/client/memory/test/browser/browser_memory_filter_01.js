/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that we can show allocation stack breakdowns in the tree.

"use strict";

const { snapshotState } = require("devtools/client/memory/constants");
const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
const { toggleInverted } = require("devtools/client/memory/actions/inverted");

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

  const filterInput = doc.getElementById("filter");
  EventUtils.synthesizeMouseAtCenter(filterInput, {}, panel.panelWin);
  EventUtils.sendString("js::Shape", panel.panelWin);

  yield waitUntilSnapshotState(store, [snapshotState.SAVING_CENSUS]);
  ok(true, "adding a filter string should trigger census recompute");

  yield waitUntilSnapshotState(store, [snapshotState.SAVED_CENSUS]);

  const nameElem = doc.querySelector(".heap-tree-item-field.heap-tree-item-name");
  ok(nameElem, "Should get a tree item row with a name");
  is(nameElem.textContent.trim(), "js::Shape", "and the tree item should be the one we filtered for");
});
