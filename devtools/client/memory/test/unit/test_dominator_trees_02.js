/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that selecting the dominator tree view automatically kicks off fetching
// and computing dominator trees.

const {
  snapshotState: states,
  dominatorTreeState,
  viewState,
} = require("devtools/client/memory/constants");
const {
  takeSnapshotAndCensus,
} = require("devtools/client/memory/actions/snapshot");
const {
  changeViewAndRefresh
} = require("devtools/client/memory/actions/view");

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

  dispatch(changeViewAndRefresh(viewState.DOMINATOR_TREE, heapWorker));
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
