/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can compute and fetch the dominator tree for a snapshot.

let {
  snapshotState: states,
  dominatorTreeState,
} = require("devtools/client/memory/constants");
let {
  takeSnapshotAndCensus,
  computeAndFetchDominatorTree,
} = require("devtools/client/memory/actions/snapshot");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  let { getState, dispatch } = store;

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS]);
  ok(!getState().snapshots[0].dominatorTree,
     "There shouldn't be a dominator tree model yet since it is not computed " +
     "until we switch to the dominators view.");

  // Change to the dominator tree view.
  dispatch(computeAndFetchDominatorTree(heapWorker, getState().snapshots[0].id));
  ok(getState().snapshots[0].dominatorTree,
     "Should now have a dominator tree model for the selected snapshot");

  // Wait for the dominator tree to start being computed.
  yield waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.state === dominatorTreeState.COMPUTING);
  ok(true, "The dominator tree started computing");
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
