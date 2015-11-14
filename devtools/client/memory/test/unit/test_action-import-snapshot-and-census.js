/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the task creator `importSnapshotAndCensus()` for the whole flow of
 * importing a snapshot, and its sub-actions.
 */

let { actions, snapshotState: states } = require("devtools/client/memory/constants");
let { breakdownEquals } = require("devtools/client/memory/utils");
let { exportSnapshot, importSnapshotAndCensus } = require("devtools/client/memory/actions/io");
let { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  let { subscribe, dispatch, getState } = store;

  let destPath = yield createTempFile();
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS]);

  let exportEvents = Promise.all([
    waitUntilAction(store, actions.EXPORT_SNAPSHOT_START),
    waitUntilAction(store, actions.EXPORT_SNAPSHOT_END)
  ]);
  dispatch(exportSnapshot(getState().snapshots[0], destPath));
  yield exportEvents;

  // Now import our freshly exported snapshot
  let i = 0;
  let expected = ["IMPORTING", "READING", "READ", "SAVING_CENSUS", "SAVED_CENSUS"];
  let expectStates = () => {
    let snapshot = getState().snapshots[1];
    if (!snapshot) {
      return;
    }
    let isCorrectState = snapshot.state === states[expected[i]];
    if (isCorrectState) {
      ok(true, `Found expected state ${expected[i]}`);
      i++;
    }
  };

  let unsubscribe = subscribe(expectStates);
  dispatch(importSnapshotAndCensus(heapWorker, destPath));

  yield waitUntilState(store, () => i === 5);
  unsubscribe();
  equal(i, 5, "importSnapshotAndCensus() produces the correct sequence of states in a snapshot");
  equal(getState().snapshots[1].state, states.SAVED_CENSUS, "imported snapshot is in SAVED_CENSUS state");
  ok(getState().snapshots[1].selected, "imported snapshot is selected");

  // Check snapshot data
  let snapshot1 = getState().snapshots[0];
  let snapshot2 = getState().snapshots[1];

  ok(breakdownEquals(snapshot1.breakdown, snapshot2.breakdown),
    "imported snapshot has correct breakdown");

  // Clone the census data so we can destructively remove the ID/parents to compare
  // equal census data
  let census1 = stripUnique(JSON.parse(JSON.stringify(snapshot1.census.report)));
  let census2 = stripUnique(JSON.parse(JSON.stringify(snapshot2.census.report)));

  equal(JSON.stringify(census1), JSON.stringify(census2), "Imported snapshot has correct census");

  function stripUnique (obj) {
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
