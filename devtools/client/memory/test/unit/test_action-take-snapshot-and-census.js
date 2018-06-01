/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the task creator `takeSnapshotAndCensus()` for the whole flow of
 * taking a snapshot, and its sub-actions.
 */

const { snapshotState: states, treeMapState } = require("devtools/client/memory/constants");
const actions = require("devtools/client/memory/actions/snapshot");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();

  let snapshotI = 0;
  let censusI = 0;
  const snapshotStates = ["SAVING", "SAVED", "READING", "READ"];
  const censusStates = ["SAVING", "SAVED"];
  const expectStates = () => {
    const snapshot = store.getState().snapshots[0];
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

  const unsubscribe = store.subscribe(expectStates);
  store.dispatch(actions.takeSnapshotAndCensus(front, heapWorker));

  await waitUntilState(store, () => {
    return snapshotI === snapshotStates.length &&
           censusI === censusStates.length;
  });
  unsubscribe();

  ok(true,
    "takeSnapshotAndCensus() produces the correct sequence of states in a snapshot");
  const snapshot = store.getState().snapshots[0];
  ok(snapshot.treeMap, "snapshot has tree map census data");
  ok(snapshot.selected, "snapshot is selected");
});
