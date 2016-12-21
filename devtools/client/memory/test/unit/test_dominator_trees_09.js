/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we can change the display with which we describe a dominator tree
// while the dominator tree is in the middle of being fetched.

const {
  dominatorTreeState,
  viewState,
  labelDisplays,
  treeMapState,
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
  yield waitUntilCensusState(store, s => s.treeMap, [treeMapState.SAVED]);
  ok(!getState().snapshots[0].dominatorTree,
     "There shouldn't be a dominator tree model yet since it is not computed " +
     "until we switch to the dominators view.");

  // Wait for the dominator tree to start fetching.
  yield waitUntilState(store, state =>
    state.snapshots[0] &&
    state.snapshots[0].dominatorTree &&
    state.snapshots[0].dominatorTree.state === dominatorTreeState.FETCHING);

  ok(getState().labelDisplay,
     "We have a default display for describing nodes in a dominator tree");
  equal(getState().labelDisplay,
        labelDisplays.coarseType,
        "and the default is coarse type");
  equal(getState().labelDisplay,
        getState().snapshots[0].dominatorTree.display,
        "and the newly computed dominator tree has that display");

  // Switch to the allocationStack display while we are still fetching the
  // dominator tree.
  dispatch(setLabelDisplayAndRefresh(
    heapWorker,
    labelDisplays.allocationStack));

  // Wait for the dominator tree to finish being fetched.
  yield waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);

  equal(getState().snapshots[0].dominatorTree.display,
        labelDisplays.allocationStack,
        "The new dominator tree's display is allocationStack");
  equal(getState().labelDisplay,
        labelDisplays.allocationStack,
        "as is our requested dominator tree display");

  heapWorker.destroy();
  yield front.detach();
});
