/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that selecting the dominator tree view and then taking a snapshot
// properly kicks off fetching and computing dominator trees.

const {
  dominatorTreeState,
  viewState,
} = require("devtools/client/memory/constants");
const {
  takeSnapshotAndCensus,
} = require("devtools/client/memory/actions/snapshot");
const {
  changeView
} = require("devtools/client/memory/actions/view");

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
  equal(getState().view.state, viewState.DOMINATOR_TREE,
        "We should now be in the DOMINATOR_TREE view");

  dispatch(takeSnapshotAndCensus(front, heapWorker));

  // Wait for the dominator tree to start being computed.
  yield waitUntilState(store, state =>
    state.snapshots[0] && state.snapshots[0].dominatorTree);
  equal(getState().snapshots[0].dominatorTree.state,
        dominatorTreeState.COMPUTING,
        "The dominator tree started computing");
  ok(!getState().snapshots[0].dominatorTree.root,
     "When the dominator tree is computing, we should not have its root");

  // Wait for the dominator tree to finish computing and start being fetched.
  yield waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.state === dominatorTreeState.FETCHING);
  ok(true, "The dominator tree started fetching");
  ok(!getState().snapshots[0].dominatorTree.root,
     "When the dominator tree is fetching, we should not have its root");

  // Wait for the dominator tree to finish being fetched.
  yield waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);
  ok(true, "The dominator tree was fetched");
  ok(getState().snapshots[0].dominatorTree.root,
     "When the dominator tree is loaded, we should have its root");

  heapWorker.destroy();
  yield front.detach();
});
