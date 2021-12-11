/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests `importSnapshotAndCensus()` when importing snapshots from the dominator
 * tree view. The snapshot is expected to be loaded and its dominator tree
 * should be computed.
 */

const {
  snapshotState,
  dominatorTreeState,
  viewState,
  treeMapState,
} = require("devtools/client/memory/constants");
const {
  importSnapshotAndCensus,
} = require("devtools/client/memory/actions/io");
const { changeViewAndRefresh } = require("devtools/client/memory/actions/view");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { subscribe, dispatch, getState } = store;

  dispatch(changeViewAndRefresh(viewState.DOMINATOR_TREE, heapWorker));
  equal(
    getState().view.state,
    viewState.DOMINATOR_TREE,
    "We should now be in the DOMINATOR_TREE view"
  );

  let i = 0;
  const expected = [
    "IMPORTING",
    "READING",
    "READ",
    "treeMap:SAVING",
    "treeMap:SAVED",
    "dominatorTree:COMPUTING",
    "dominatorTree:FETCHING",
    "dominatorTree:LOADED",
  ];
  const expectStates = () => {
    const snapshot = getState().snapshots[0];
    if (snapshot && hasExpectedState(snapshot, expected[i])) {
      ok(true, `Found expected state ${expected[i]}`);
      i++;
    }
  };

  const unsubscribe = subscribe(expectStates);
  const snapshotPath = await front.saveHeapSnapshot();
  dispatch(importSnapshotAndCensus(heapWorker, snapshotPath));

  await waitUntilState(store, () => i === expected.length);
  unsubscribe();
  equal(
    i,
    expected.length,
    "importSnapshotAndCensus() produces the correct " +
      "sequence of states in a snapshot"
  );
  equal(
    getState().snapshots[0].dominatorTree.state,
    dominatorTreeState.LOADED,
    "imported snapshot's dominator tree is in LOADED state"
  );
  ok(getState().snapshots[0].selected, "imported snapshot is selected");
});

/**
 * Check that the provided snapshot is in the expected state. The expected state
 * is a snapshotState by default. If the expected state is prefixed by
 * dominatorTree, a dominatorTree is expected on the provided snapshot, in the
 * corresponding state from dominatorTreeState.
 */
function hasExpectedState(snapshot, expectedState) {
  const isDominatorState = expectedState.indexOf("dominatorTree:") === 0;
  if (isDominatorState) {
    const state =
      dominatorTreeState[expectedState.replace("dominatorTree:", "")];
    return snapshot.dominatorTree && snapshot.dominatorTree.state === state;
  }

  const isTreeMapState = expectedState.indexOf("treeMap:") === 0;
  if (isTreeMapState) {
    const state = treeMapState[expectedState.replace("treeMap:", "")];
    return snapshot.treeMap && snapshot.treeMap.state === state;
  }

  const state = snapshotState[expectedState];
  return snapshot.state === state;
}
