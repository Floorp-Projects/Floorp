/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the task creator `importSnapshotAndCensus()` for the whole flow of
 * importing a snapshot, and its sub-actions.
 */

const { actions, snapshotState: states, treeMapState } = require("devtools/client/memory/constants");
const { exportSnapshot, importSnapshotAndCensus } = require("devtools/client/memory/actions/io");
const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { subscribe, dispatch, getState } = store;

  const destPath = await createTempFile();
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  await waitUntilCensusState(store, s => s.treeMap, [treeMapState.SAVED]);

  const exportEvents = Promise.all([
    waitUntilAction(store, actions.EXPORT_SNAPSHOT_START),
    waitUntilAction(store, actions.EXPORT_SNAPSHOT_END)
  ]);
  dispatch(exportSnapshot(getState().snapshots[0], destPath));
  await exportEvents;

  // Now import our freshly exported snapshot
  let snapshotI = 0;
  let censusI = 0;
  const snapshotStates = ["IMPORTING", "READING", "READ"];
  const censusStates = ["SAVING", "SAVED"];
  const expectStates = () => {
    const snapshot = getState().snapshots[1];
    if (!snapshot) {
      return;
    }
    if (snapshotI < snapshotStates.length) {
      const isCorrectState = snapshot.state === states[snapshotStates[snapshotI]];
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

  const unsubscribe = subscribe(expectStates);
  dispatch(importSnapshotAndCensus(heapWorker, destPath));

  await waitUntilState(store, () => {
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
  const snapshot1 = getState().snapshots[0];
  const snapshot2 = getState().snapshots[1];

  equal(snapshot1.treeMap.display, snapshot2.treeMap.display,
        "imported snapshot has correct display");

  // Clone the census data so we can destructively remove the ID/parents to compare
  // equal census data
  const census1 = stripUnique(JSON.parse(JSON.stringify(snapshot1.treeMap.report)));
  const census2 = stripUnique(JSON.parse(JSON.stringify(snapshot2.treeMap.report)));

  equal(JSON.stringify(census1), JSON.stringify(census2),
    "Imported snapshot has correct census");

  function stripUnique(obj) {
    const children = obj.children || [];
    for (const child of children) {
      delete child.id;
      delete child.parent;
      stripUnique(child);
    }
    delete obj.id;
    delete obj.parent;
    return obj;
  }
});
