/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the task creator `takeSnapshotAndCensus()` for the whole flow of
 * taking a snapshot, and its sub-actions.
 */

let { snapshotState: states, treeMapState } = require("devtools/client/memory/constants");
let actions = require("devtools/client/memory/actions/snapshot");

function run_test() {
  run_next_test();
}

add_task(function* () {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();

  let snapshotI = 0;
  let censusI = 0;
  let snapshotStates = ["SAVING", "SAVED", "READING", "READ"];
  let censusStates = ["SAVING", "SAVED"];
  let expectStates = () => {
    let snapshot = store.getState().snapshots[0];
    if (!snapshot) {
      return;
    }
    if (snapshotI < snapshotStates.length) {
      let isCorrectState = snapshot.state === states[snapshotStates[snapshotI]];
      if (isCorrectState) {
        ok(true, `Found expected snapshot state ${snapshotStates[snapshotI]}`);
        snapshotI++;
      }
    }
    if (snapshot.treeMap && censusI < censusStates.length) {
      if (snapshot.treeMap.state === treeMapState[censusStates[censusI]]) {
        ok(true, `Found expected census state ${censusStates[censusI]}`);
        censusI++;
      }
    }
  };

  let unsubscribe = store.subscribe(expectStates);
  store.dispatch(actions.takeSnapshotAndCensus(front, heapWorker));

  yield waitUntilState(store, () => {
    return snapshotI === snapshotStates.length &&
           censusI === censusStates.length;
  });
  unsubscribe();

  ok(true,
    "takeSnapshotAndCensus() produces the correct sequence of states in a snapshot");
  let snapshot = store.getState().snapshots[0];
  ok(snapshot.treeMap, "snapshot has tree map census data");
  ok(snapshot.selected, "snapshot is selected");
});
