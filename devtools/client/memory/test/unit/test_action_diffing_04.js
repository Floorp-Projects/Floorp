/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we compute census diffs.

const {
  diffingState,
  snapshotState
} = require("devtools/client/memory/constants");
const {
  toggleDiffing,
  selectSnapshotForDiffingAndRefresh
} = require("devtools/client/memory/actions/diffing");
const {
  takeSnapshot,
  readSnapshot
} = require("devtools/client/memory/actions/snapshot");
const { breakdownEquals } = require("devtools/client/memory/utils");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  const { getState, dispatch } = store;

  equal(getState().diffing, null, "not diffing by default");

  const s1 = yield dispatch(takeSnapshot(front, heapWorker));
  const s2 = yield dispatch(takeSnapshot(front, heapWorker));
  const s3 = yield dispatch(takeSnapshot(front, heapWorker));
  dispatch(readSnapshot(heapWorker, s1));
  dispatch(readSnapshot(heapWorker, s2));
  dispatch(readSnapshot(heapWorker, s3));
  yield waitUntilSnapshotState(store, [snapshotState.READ,
                                       snapshotState.READ,
                                       snapshotState.READ]);

  dispatch(toggleDiffing());
  dispatch(selectSnapshotForDiffingAndRefresh(heapWorker,
                                              getState().snapshots[0]));
  dispatch(selectSnapshotForDiffingAndRefresh(heapWorker,
                                              getState().snapshots[1]));

  ok(getState().diffing, "We should be diffing.");
  equal(getState().diffing.firstSnapshotId, getState().snapshots[0].id,
        "First snapshot selected.");
  equal(getState().diffing.secondSnapshotId, getState().snapshots[1].id,
        "Second snapshot selected.");

  yield waitUntilState(store,
                       state =>
                         state.diffing.state === diffingState.TAKING_DIFF);
  ok(true,
     "Selecting two snapshots for diffing should trigger computing a diff.");

  yield waitUntilState(store,
                       state => state.diffing.state === diffingState.TOOK_DIFF);
  ok(true, "And then the diff should complete.");
  ok(getState().diffing.census, "And we should have a census.");
  ok(getState().diffing.census.report, "And that census should have a report.");
  ok(breakdownEquals(getState().diffing.census.breakdown, getState().breakdown),
     "And that census should have the correct breakdown");
  equal(getState().diffing.census.filter, getState().filter,
        "And that census should have the correct filter");
  equal(getState().diffing.census.inverted, getState().inverted,
        "And that census should have the correct inversion");

  heapWorker.destroy();
  yield front.detach();
});
