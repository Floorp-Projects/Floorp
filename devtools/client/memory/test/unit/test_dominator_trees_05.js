/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changing the currently selected snapshot to a snapshot that does
// not have a dominator tree will automatically compute and fetch one for it.

const {
  dominatorTreeState,
  viewState,
  treeMapState,
} = require("devtools/client/memory/constants");
const {
  takeSnapshotAndCensus,
  selectSnapshotAndRefresh,
} = require("devtools/client/memory/actions/snapshot");

const { changeView } = require("devtools/client/memory/actions/view");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  await waitUntilCensusState(store, s => s.treeMap,
                             [treeMapState.SAVED, treeMapState.SAVED]);

  ok(getState().snapshots[1].selected, "The second snapshot is selected");

  // Change to the dominator tree view.
  dispatch(changeView(viewState.DOMINATOR_TREE));

  // Wait for the dominator tree to finish being fetched.
  await waitUntilState(store, state =>
    state.snapshots[1].dominatorTree &&
    state.snapshots[1].dominatorTree.state === dominatorTreeState.LOADED);
  ok(true, "The second snapshot's dominator tree was fetched");

  // Select the first snapshot.
  dispatch(selectSnapshotAndRefresh(heapWorker, getState().snapshots[0].id));

  // And now the first snapshot should have its dominator tree fetched and
  // computed because of the new selection.
  await waitUntilState(store, state =>
    state.snapshots[0].dominatorTree &&
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);
  ok(true, "The first snapshot's dominator tree was fetched");

  heapWorker.destroy();
  await front.detach();
});
