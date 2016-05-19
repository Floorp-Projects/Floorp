/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can incrementally fetch a subtree of a dominator tree.

const {
  snapshotState: states,
  dominatorTreeState,
  viewState,
} = require("devtools/client/memory/constants");
const {
  takeSnapshotAndCensus,
  selectSnapshotAndRefresh,
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

  // Find a node that has children, but none of them are loaded.

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
  ok(oldNode,
     "Should have found a node with children that are not loaded since we " +
     "only send partial dominator trees across initially and load the rest " +
     "on demand");
  ok(oldNode !== oldRoot, "But the node should not be the root");

  const lazyChildren = new DominatorTreeLazyChildren(oldNode.nodeId, 0);
  dispatch(fetchImmediatelyDominated(heapWorker, getState().snapshots[0].id, lazyChildren));

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
  equal(oldRoot.children.length, newRoot.children.length,
        "The new tree's root should have the same number of children as the " +
        "old root's");

  let differentChildrenCount = 0;
  for (let i = 0; i < oldRoot.children.length; i++) {
    if (oldRoot.children[i] !== newRoot.children[i]) {
      differentChildrenCount++;
    }
  }
  equal(differentChildrenCount, 1,
        "All subtrees except the subtree we inserted incrementally fetched " +
        "children into should be the same because we use persistent updates");

  // Find the new node which has the children inserted.

  function findNewNode(node) {
    if (node.nodeId === oldNode.nodeId) {
      return node;
    }

    if (node.children) {
      for (let child of node.children) {
        const found = findNewNode(child);
        if (found) {
          return found;
        }
      }
    }

    return null;
  }

  const newNode = findNewNode(newRoot);
  ok(newNode, "Should find the node in the new tree again");
  ok(newNode !== oldNode, "We did not mutate the old node in place, instead created a new node");
  ok(newNode.children, "And the new node should have the children attached");

  heapWorker.destroy();
  yield front.detach();
});
