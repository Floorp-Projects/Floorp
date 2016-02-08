/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Integration test for mouse interaction in the dominator tree

"use strict";

const {
  dominatorTreeState,
  viewState,
} = require("devtools/client/memory/constants");
const { changeView } = require("devtools/client/memory/actions/view");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

function clickOnNodeArrow(node, panel) {
  EventUtils.synthesizeMouseAtCenter(node.querySelector(".arrow"),
                                     {}, panel.panelWin);
}

this.test = makeMemoryTest(TEST_URL, function* ({ panel }) {
  // Taking snapshots and computing dominator trees is slow :-/
  requestLongerTimeout(4);

  const store = panel.panelWin.gStore;
  const { getState, dispatch } = store;
  const doc = panel.panelWin.document;

  dispatch(changeView(viewState.DOMINATOR_TREE));

  // Take a snapshot.
  const takeSnapshotButton = doc.getElementById("take-snapshot");
  EventUtils.synthesizeMouseAtCenter(takeSnapshotButton, {}, panel.panelWin);

  // Wait for the dominator tree to be computed and fetched.
  yield waitUntilState(store, state =>
    state.snapshots[0] &&
    state.snapshots[0].dominatorTree &&
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);
  ok(true, "Computed and fetched the dominator tree.");

  const root = getState().snapshots[0].dominatorTree.root;
  ok(getState().snapshots[0].dominatorTree.expanded.has(root.nodeId),
     "Root node is expanded by default");

  // Click on root arrow to collapse the root element
  const rootNode = doc.querySelector(`.node-${root.nodeId}`);
  clickOnNodeArrow(rootNode, panel);

  yield waitUntilState(store, state =>
    state.snapshots[0] &&
    state.snapshots[0].dominatorTree &&
    !state.snapshots[0].dominatorTree.expanded.has(root.nodeId));
  ok(true, "Root node collapsed");

  // Click on root arrow to expand it again
  clickOnNodeArrow(rootNode, panel);

  yield waitUntilState(store, state =>
    state.snapshots[0] &&
    state.snapshots[0].dominatorTree &&
    state.snapshots[0].dominatorTree.expanded.has(root.nodeId));
  ok(true, "Root node is expanded again");
});
