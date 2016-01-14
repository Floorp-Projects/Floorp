/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can change the breakdown with which we describe a dominator tree
// and that the dominator tree is re-fetched.

const {
  snapshotState: states,
  dominatorTreeState,
  viewState,
  dominatorTreeBreakdowns,
} = require("devtools/client/memory/constants");
const {
  setDominatorTreeBreakdownAndRefresh
} = require("devtools/client/memory/actions/dominatorTreeBreakdown");
const {
  changeView,
} = require("devtools/client/memory/actions/view");
const {
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

  dispatch(changeView(viewState.DOMINATOR_TREE));

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS]);
  ok(!getState().snapshots[0].dominatorTree,
     "There shouldn't be a dominator tree model yet since it is not computed " +
     "until we switch to the dominators view.");

  // Wait for the dominator tree to finish being fetched.
  yield waitUntilState(store, state =>
    state.snapshots[0] &&
    state.snapshots[0].dominatorTree &&
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);

  ok(getState().dominatorTreeBreakdown,
     "We have a default breakdown for describing nodes in a dominator tree");
  equal(getState().dominatorTreeBreakdown,
        dominatorTreeBreakdowns.coarseType.breakdown,
        "and the default is coarse type");
  equal(getState().dominatorTreeBreakdown,
        getState().snapshots[0].dominatorTree.breakdown,
        "and the newly computed dominator tree has that breakdown");

  // Switch to the internalType breakdown.
  dispatch(setDominatorTreeBreakdownAndRefresh(
    heapWorker,
    dominatorTreeBreakdowns.internalType.breakdown));

  yield waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.state === dominatorTreeState.FETCHING);
  ok(true,
     "switching breakdown types caused the dominator tree to be fetched " +
     "again.");

  yield waitUntilState(store, state =>
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);
  equal(getState().snapshots[0].dominatorTree.breakdown,
        dominatorTreeBreakdowns.internalType.breakdown,
        "The new dominator tree's breakdown is internalType");
  equal(getState().dominatorTreeBreakdown,
        dominatorTreeBreakdowns.internalType.breakdown,
        "as is our requested dominator tree breakdown");

  heapWorker.destroy();
  yield front.detach();
});
