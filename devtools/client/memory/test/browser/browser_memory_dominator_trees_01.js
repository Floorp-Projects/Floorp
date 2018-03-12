/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test for dominator trees, their focused nodes, and keyboard navigating
// through nodes across incrementally fetching subtrees.

"use strict";

const {
  dominatorTreeState,
  viewState,
} = require("devtools/client/memory/constants");
const {
  expandDominatorTreeNode,
} = require("devtools/client/memory/actions/snapshot");
const { changeView } = require("devtools/client/memory/actions/view");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_big_tree.html";

this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
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

  yield waitUntilDominatorTreeState(store, [dominatorTreeState.LOADED]);
  ok(true, "Computed and fetched the dominator tree.");

  // Expand all the dominator tree nodes that are eagerly fetched, except for
  // the leaves which will trigger fetching their lazily loaded subtrees.

  const id = getState().snapshots[0].id;
  const root = getState().snapshots[0].dominatorTree.root;
  (function expandAllEagerlyFetched(node = root) {
    if (!node.moreChildrenAvailable || node.children) {
      dispatch(expandDominatorTreeNode(id, node));
    }

    if (node.children) {
      for (let child of node.children) {
        expandAllEagerlyFetched(child);
      }
    }
  }());

  // Find the deepest eagerly loaded node: one which has more children but none
  // of them are loaded.

  const deepest = (function findDeepest(node = root) {
    if (node.moreChildrenAvailable && !node.children) {
      return node;
    }

    if (node.children) {
      for (let child of node.children) {
        const found = findDeepest(child);
        if (found) {
          return found;
        }
      }
    }

    return null;
  }());

  ok(deepest, "Found the deepest node");
  ok(!getState().snapshots[0].dominatorTree.expanded.has(deepest.nodeId),
     "The deepest node should not be expanded");

  // Select the deepest node.

  EventUtils.synthesizeMouseAtCenter(doc.querySelector(`.node-${deepest.nodeId}`),
                                     {},
                                     panel.panelWin);
  yield waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.focused.nodeId === deepest.nodeId);
  ok(doc.querySelector(`.node-${deepest.nodeId}`).classList.contains("focused"),
     "The deepest node should be focused now");

  // Expand the deepest node, which triggers an incremental fetch of its lazily
  // loaded subtree.

  EventUtils.synthesizeKey("VK_RIGHT", {}, panel.panelWin);
  yield waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.expanded.has(deepest.nodeId));
  is(getState().snapshots[0].dominatorTree.state,
     dominatorTreeState.INCREMENTAL_FETCHING,
     "Expanding the deepest node should start an incremental fetch of its subtree");
  ok(doc.querySelector(`.node-${deepest.nodeId}`).classList.contains("focused"),
     "The deepest node should still be focused after expansion");

  // Wait for the incremental fetch to complete.

  yield waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);
  ok(true, "And the incremental fetch completes.");
  ok(doc.querySelector(`.node-${deepest.nodeId}`).classList.contains("focused"),
     "The deepest node should still be focused after we have loaded its children");

  // Find the most up-to-date version of the node whose children we just
  // incrementally fetched.

  const newDeepest = (function findNewDeepest(node = getState().snapshots[0]
                                                               .dominatorTree
                                                               .root) {
    if (node.nodeId === deepest.nodeId) {
      return node;
    }

    if (node.children) {
      for (let child of node.children) {
        const found = findNewDeepest(child);
        if (found) {
          return found;
        }
      }
    }

    return null;
  }());

  ok(newDeepest, "We found the up-to-date version of deepest");
  ok(newDeepest.children, "And its children are loaded");
  ok(newDeepest.children.length, "And there are more than 0 children");

  const firstChild = newDeepest.children[0];
  ok(firstChild, "deepest should have a first child");
  ok(doc.querySelector(`.node-${firstChild.nodeId}`),
     "and the first child should exist in the dom");

  // Select the newly loaded first child by pressing the right arrow once more.

  EventUtils.synthesizeKey("VK_RIGHT", {}, panel.panelWin);
  yield waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.focused === firstChild);
  ok(doc.querySelector(`.node-${firstChild.nodeId}`).classList.contains("focused"),
     "The first child should now be focused");
});
