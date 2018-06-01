/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we maintain focus of the selected dominator tree node across
// changing breakdowns for labeling them.

const {
  dominatorTreeState,
  labelDisplays,
  viewState,
} = require("devtools/client/memory/constants");
const {
  takeSnapshotAndCensus,
  focusDominatorTreeNode,
} = require("devtools/client/memory/actions/snapshot");
const {
  changeView,
} = require("devtools/client/memory/actions/view");
const {
  setLabelDisplayAndRefresh,
} = require("devtools/client/memory/actions/label-display");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  dispatch(changeView(viewState.DOMINATOR_TREE));
  dispatch(takeSnapshotAndCensus(front, heapWorker));

  // Wait for the dominator tree to finish being fetched.
  await waitUntilState(store, state =>
    state.snapshots[0] &&
    state.snapshots[0].dominatorTree &&
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);
  ok(true, "The dominator tree was fetched");

  const root = getState().snapshots[0].dominatorTree.root;
  ok(root, "When the dominator tree is loaded, we should have its root");

  dispatch(focusDominatorTreeNode(getState().snapshots[0].id, root));
  equal(root, getState().snapshots[0].dominatorTree.focused,
        "The root should be focused.");

  equal(getState().labelDisplay, labelDisplays.coarseType,
        "Using labelDisplays.coarseType by default");
  dispatch(setLabelDisplayAndRefresh(heapWorker,
                                             labelDisplays.allocationStack));
  equal(getState().labelDisplay, labelDisplays.allocationStack,
        "Using labelDisplays.allocationStack now");

  await waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.state === dominatorTreeState.FETCHING);
  ok(true, "We started re-fetching the dominator tree");

  await waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);
  ok(true, "The dominator tree was loaded again");

  ok(getState().snapshots[0].dominatorTree.focused,
     "Still have a focused node");
  equal(getState().snapshots[0].dominatorTree.focused.nodeId, root.nodeId,
        "Focused node is the same as before");

  heapWorker.destroy();
  await front.detach();
});
