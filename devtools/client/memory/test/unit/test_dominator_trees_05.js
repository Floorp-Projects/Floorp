/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changing the currently selected snapshot to a snapshot that does
// not have a dominator tree will automatically compute and fetch one for it.

let {
  dominatorTreeState,
  viewState,
  treeMapState,
} = require("devtools/client/memory/constants");
let {
  takeSnapshotAndCensus,
  selectSnapshotAndRefresh,
} = require("devtools/client/memory/actions/snapshot");

let { changeView } = require("devtools/client/memory/actions/view");

function run_test() {
  run_next_test();
}

add_task(function* () {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  let { getState, dispatch } = store;

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilCensusState(store, s => s.treeMap,
                             [treeMapState.SAVED, treeMapState.SAVED]);

  ok(getState().snapshots[1].selected, "The second snapshot is selected");

  // Change to the dominator tree view.
  dispatch(changeView(viewState.DOMINATOR_TREE));

  // Wait for the dominator tree to finish being fetched.
  yield waitUntilState(store, state =>
    state.snapshots[1].dominatorTree &&
    state.snapshots[1].dominatorTree.state === dominatorTreeState.LOADED);
  ok(true, "The second snapshot's dominator tree was fetched");

  // Select the first snapshot.
  dispatch(selectSnapshotAndRefresh(heapWorker, getState().snapshots[0].id));

  // And now the first snapshot should have its dominator tree fetched and
  // computed because of the new selection.
  yield waitUntilState(store, state =>
    state.snapshots[0].dominatorTree &&
    state.snapshots[0].dominatorTree.state === dominatorTreeState.LOADED);
  ok(true, "The first snapshot's dominator tree was fetched");

  heapWorker.destroy();
  yield front.detach();
});
