/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that selecting the dominator tree view while in the middle of taking a
// snapshot properly kicks off fetching and computing dominator trees.

const {
  snapshotState: states,
  dominatorTreeState,
  viewState,
} = require("devtools/client/memory/constants");
const {
  takeSnapshotAndCensus,
} = require("devtools/client/memory/actions/snapshot");
const {
  changeView
} = require("devtools/client/memory/actions/view");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();

  for (const intermediateSnapshotState of [states.SAVING, states.READING, states.READ]) {
    dumpn("Testing switching to the DOMINATOR_TREE view in the middle of the " +
          `${intermediateSnapshotState} snapshot state`);

    const store = Store();
    const { getState, dispatch } = store;

    dispatch(takeSnapshotAndCensus(front, heapWorker));
    await waitUntilSnapshotState(store, [intermediateSnapshotState]);

    dispatch(changeView(viewState.DOMINATOR_TREE));
    equal(getState().view.state, viewState.DOMINATOR_TREE,
          "We should now be in the DOMINATOR_TREE view");

    // Wait for the dominator tree to start being computed.
    await waitUntilState(store, state =>
      state.snapshots[0] && state.snapshots[0].dominatorTree);
    equal(getState().snapshots[0].dominatorTree.state,
          dominatorTreeState.COMPUTING,
          "The dominator tree started computing");
    ok(!getState().snapshots[0].dominatorTree.root,
       "When the dominator tree is computing, we should not have its root");

    // Wait for the dominator tree to finish computing and start being fetched.
    await waitUntilState(store, state =>
      state.snapshots[0].dominatorTree.state === dominatorTreeState.FETCHING);
    ok(true, "The dominator tree started fetching");
    ok(!getState().snapshots[0].dominatorTree.root,
       "When the dominator tree is fetching, we should not have its root");

    // Wait for the dominator tree to finish being fetched.
    await waitUntilState(store, state =>
      state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);
    ok(true, "The dominator tree was fetched");
    ok(getState().snapshots[0].dominatorTree.root,
       "When the dominator tree is loaded, we should have its root");
  }

  heapWorker.destroy();
  await front.detach();
});
