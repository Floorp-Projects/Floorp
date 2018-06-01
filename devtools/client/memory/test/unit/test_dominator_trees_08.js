/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we can change the display with which we describe a dominator tree
// and that the dominator tree is re-fetched.

const {
  dominatorTreeState,
  viewState,
  labelDisplays,
  treeMapState
} = require("devtools/client/memory/constants");
const {
  setLabelDisplayAndRefresh
} = require("devtools/client/memory/actions/label-display");
const {
  changeView,
} = require("devtools/client/memory/actions/view");
const {
  takeSnapshotAndCensus,
} = require("devtools/client/memory/actions/snapshot");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  dispatch(changeView(viewState.DOMINATOR_TREE));

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  await waitUntilCensusState(store, s => s.treeMap, [treeMapState.SAVED]);
  ok(!getState().snapshots[0].dominatorTree,
     "There shouldn't be a dominator tree model yet since it is not computed " +
     "until we switch to the dominators view.");

  // Wait for the dominator tree to finish being fetched.
  await waitUntilState(store, state =>
    state.snapshots[0] &&
    state.snapshots[0].dominatorTree &&
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);

  ok(getState().labelDisplay,
     "We have a default display for describing nodes in a dominator tree");
  equal(getState().labelDisplay,
        labelDisplays.coarseType,
        "and the default is coarse type");
  equal(getState().labelDisplay,
        getState().snapshots[0].dominatorTree.display,
        "and the newly computed dominator tree has that display");

  // Switch to the allocationStack display.
  dispatch(setLabelDisplayAndRefresh(
    heapWorker,
    labelDisplays.allocationStack));

  await waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.state === dominatorTreeState.FETCHING);
  ok(true,
     "switching display types caused the dominator tree to be fetched " +
     "again.");

  await waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);
  equal(getState().snapshots[0].dominatorTree.display,
        labelDisplays.allocationStack,
        "The new dominator tree's display is allocationStack");
  equal(getState().labelDisplay,
        labelDisplays.allocationStack,
        "as is our requested dominator tree display");

  heapWorker.destroy();
  await front.detach();
});
