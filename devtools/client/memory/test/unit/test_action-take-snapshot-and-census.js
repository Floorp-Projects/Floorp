/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the task creator `takeSnapshotAndCensus()` for the whole flow of
 * taking a snapshot, and its sub-actions.
 */

let { snapshotState: states } = require("devtools/client/memory/constants");
let actions = require("devtools/client/memory/actions/snapshot");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();

  let i = 0;
  let expected = ["SAVING", "SAVED", "READING", "READ", "SAVING_CENSUS", "SAVED_CENSUS"];
  let expectStates = () => {
    if (i >= expected.length) { return false; }

    let snapshot = store.getState().snapshots[0] || {};
    let isCorrectState = snapshot.state === states[expected[i]];
    if (isCorrectState) {
      ok(true, `Found expected state ${expected[i]}`);
      i++;
    }
    return isCorrectState;
  };

  let unsubscribe = store.subscribe(expectStates);
  store.dispatch(actions.takeSnapshotAndCensus(front, heapWorker));

  yield waitUntilState(store, () => i === 6);
  unsubscribe();

  ok(true, "takeSnapshotAndCensus() produces the correct sequence of states in a snapshot");
  let snapshot = store.getState().snapshots[0];
  ok(snapshot.census, "snapshot has census data");
  ok(snapshot.selected, "snapshot is selected");
});
