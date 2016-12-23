/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the task creator `importSnapshotAndCensus()` for the whole flow of
 * importing a snapshot, and its sub-actions.
 */

let { actions, snapshotState: states, treeMapState } = require("devtools/client/memory/constants");
let { exportSnapshot, importSnapshotAndCensus } = require("devtools/client/memory/actions/io");
let { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");

function run_test() {
  run_next_test();
}

add_task(function* () {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  let { subscribe, dispatch, getState } = store;

  let destPath = yield createTempFile();
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilCensusState(store, s => s.treeMap, [treeMapState.SAVED]);

  let exportEvents = Promise.all([
    waitUntilAction(store, actions.EXPORT_SNAPSHOT_START),
    waitUntilAction(store, actions.EXPORT_SNAPSHOT_END)
  ]);
  dispatch(exportSnapshot(getState().snapshots[0], destPath));
  yield exportEvents;

  // Now import our freshly exported snapshot
  let snapshotI = 0;
  let censusI = 0;
  let snapshotStates = ["IMPORTING", "READING", "READ"];
  let censusStates = ["SAVING", "SAVED"];
  let expectStates = () => {
    let snapshot = getState().snapshots[1];
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

  let unsubscribe = subscribe(expectStates);
  dispatch(importSnapshotAndCensus(heapWorker, destPath));

  yield waitUntilState(store, () => {
    return snapshotI === snapshotStates.length &&
           censusI === censusStates.length;
  });
  unsubscribe();
  equal(snapshotI, snapshotStates.length,
    "importSnapshotAndCensus() produces the correct sequence of states in a snapshot");
  equal(getState().snapshots[1].state, states.READ,
    "imported snapshot is in READ state");
  equal(censusI, censusStates.length,
    "importSnapshotAndCensus() produces the correct sequence of states in a census");
  equal(getState().snapshots[1].treeMap.state, treeMapState.SAVED,
    "imported snapshot is in READ state");
  ok(getState().snapshots[1].selected, "imported snapshot is selected");

  // Check snapshot data
  let snapshot1 = getState().snapshots[0];
  let snapshot2 = getState().snapshots[1];

  equal(snapshot1.treeMap.display, snapshot2.treeMap.display,
        "imported snapshot has correct display");

  // Clone the census data so we can destructively remove the ID/parents to compare
  // equal census data
  let census1 = stripUnique(JSON.parse(JSON.stringify(snapshot1.treeMap.report)));
  let census2 = stripUnique(JSON.parse(JSON.stringify(snapshot2.treeMap.report)));

  equal(JSON.stringify(census1), JSON.stringify(census2),
    "Imported snapshot has correct census");

  function stripUnique(obj) {
    let children = obj.children || [];
    for (let child of children) {
      delete child.id;
      delete child.parent;
      stripUnique(child);
    }
    delete obj.id;
    delete obj.parent;
    return obj;
  }
});
