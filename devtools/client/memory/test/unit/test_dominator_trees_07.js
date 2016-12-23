/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we can incrementally fetch two subtrees in the same dominator tree
// concurrently. This exercises the activeFetchRequestCount machinery.

const {
  dominatorTreeState,
  viewState,
} = require("devtools/client/memory/constants");
const {
  takeSnapshotAndCensus,
  fetchImmediatelyDominated,
} = require("devtools/client/memory/actions/snapshot");
const DominatorTreeLazyChildren
  = require("devtools/client/memory/dominator-tree-lazy-children");

const { changeView } = require("devtools/client/memory/actions/view");

function run_test() {
  run_next_test();
}

add_task(function* () {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  let { getState, dispatch } = store;

  dispatch(changeView(viewState.DOMINATOR_TREE));
  dispatch(takeSnapshotAndCensus(front, heapWorker));

  // Wait for the dominator tree to finish being fetched.
  yield waitUntilState(store, state =>
    state.snapshots[0] &&
    state.snapshots[0].dominatorTree &&
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);
  ok(getState().snapshots[0].dominatorTree.root,
     "The dominator tree was fetched");

  // Find a node that has more children.

  function findNode(node) {
    if (node.moreChildrenAvailable && !node.children) {
      return node;
    }

    if (node.children) {
      for (let child of node.children) {
        const found = findNode(child);
        if (found) {
          return found;
        }
      }
    }

    return null;
  }

  const oldRoot = getState().snapshots[0].dominatorTree.root;
  const oldNode = findNode(oldRoot);
  ok(oldNode, "Should have found a node with more children.");

  // Find another node that has more children.
  function findNodeRev(node) {
    if (node.moreChildrenAvailable && !node.children) {
      return node;
    }

    if (node.children) {
      for (let child of node.children.slice().reverse()) {
        const found = findNodeRev(child);
        if (found) {
          return found;
        }
      }
    }

    return null;
  }

  const oldNode2 = findNodeRev(oldRoot);
  ok(oldNode2, "Should have found another node with more children.");
  ok(oldNode !== oldNode2,
     "The second node should not be the same as the first one");

  // Fetch both subtrees concurrently.
  dispatch(fetchImmediatelyDominated(heapWorker, getState().snapshots[0].id,
                                     new DominatorTreeLazyChildren(oldNode.nodeId, 0)));
  dispatch(fetchImmediatelyDominated(heapWorker, getState().snapshots[0].id,
                                     new DominatorTreeLazyChildren(oldNode2.nodeId, 0)));

  equal(getState().snapshots[0].dominatorTree.state,
        dominatorTreeState.INCREMENTAL_FETCHING,
        "Fetching immediately dominated children should put us in the " +
        "INCREMENTAL_FETCHING state");

  yield waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);
  ok(true,
     "The dominator tree should go back to LOADED after the incremental " +
     "fetching is done.");

  const newRoot = getState().snapshots[0].dominatorTree.root;
  ok(oldRoot !== newRoot,
     "When we insert new nodes, we get a new tree");

  // Find the new node which has the children inserted.

  function findNodeWithId(id, node) {
    if (node.nodeId === id) {
      return node;
    }

    if (node.children) {
      for (let child of node.children) {
        const found = findNodeWithId(id, child);
        if (found) {
          return found;
        }
      }
    }

    return null;
  }

  const newNode = findNodeWithId(oldNode.nodeId, newRoot);
  ok(newNode, "Should find the node in the new tree again");
  ok(newNode !== oldNode,
     "We did not mutate the old node in place, instead created a new node");
  ok(newNode.children.length,
     "And the new node should have the new children attached");

  const newNode2 = findNodeWithId(oldNode2.nodeId, newRoot);
  ok(newNode2, "Should find the second node in the new tree again");
  ok(newNode2 !== oldNode2,
     "We did not mutate the second old node in place, instead created a new node");
  ok(newNode2.children,
     "And the new node should have the new children attached");

  heapWorker.destroy();
  yield front.detach();
});
