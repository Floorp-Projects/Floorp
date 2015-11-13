/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test selecting snapshots for diffing.

const { diffingState, snapshotState } = require("devtools/client/memory/constants");
const {
  toggleDiffing,
  selectSnapshotForDiffing
} = require("devtools/client/memory/actions/diffing");
const { takeSnapshot } = require("devtools/client/memory/actions/snapshot");

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

  dispatch(takeSnapshot(front, heapWorker));
  dispatch(takeSnapshot(front, heapWorker));
  dispatch(takeSnapshot(front, heapWorker));
  yield waitUntilSnapshotState(store, [snapshotState.SAVED,
                                       snapshotState.SAVED,
                                       snapshotState.SAVED]);
  dispatch(takeSnapshot(front));

  // Start diffing.
  dispatch(toggleDiffing());
  ok(getState().diffing, "now diffing after toggling");
  equal(getState().diffing.firstSnapshotId, null,
        "no first snapshot selected");
  equal(getState().diffing.secondSnapshotId, null,
        "no second snapshot selected");
  equal(getState().diffing.state, diffingState.SELECTING,
        "should be in diffing state SELECTING");

  // Can't select a snapshot that is not in a diffable state.
  equal(getState().snapshots[3].state, snapshotState.SAVING,
        "the last snapshot is still in the process of being saved");
  dumpn("Expecting exception:");
  let threw = false;
  try {
    dispatch(selectSnapshotForDiffing(getState().snapshots[3]));
  } catch (error) {
    threw = true;
  }
  ok(threw, "Should not be able to select snapshots that aren't ready for diffing");

  // Select first snapshot for diffing.
  dispatch(selectSnapshotForDiffing(getState().snapshots[0]));
  ok(getState().diffing, "now diffing after toggling");
  equal(getState().diffing.firstSnapshotId, getState().snapshots[0].id,
        "first snapshot selected");
  equal(getState().diffing.secondSnapshotId, null,
        "no second snapshot selected");
  equal(getState().diffing.state, diffingState.SELECTING,
        "should still be in diffing state SELECTING");

  // Can't diff first snapshot with itself; this is a noop.
  dispatch(selectSnapshotForDiffing(getState().snapshots[0]));
  ok(getState().diffing, "still diffing");
  equal(getState().diffing.firstSnapshotId, getState().snapshots[0].id,
        "first snapshot still selected");
  equal(getState().diffing.secondSnapshotId, null,
        "still no second snapshot selected");
  equal(getState().diffing.state, diffingState.SELECTING,
        "should still be in diffing state SELECTING");

  // Select second snapshot for diffing.
  dispatch(selectSnapshotForDiffing(getState().snapshots[1]));
  ok(getState().diffing, "still diffing");
  equal(getState().diffing.firstSnapshotId, getState().snapshots[0].id,
        "first snapshot still selected");
  equal(getState().diffing.secondSnapshotId, getState().snapshots[1].id,
        "second snapshot selected");

  // Can't select more than two snapshots for diffing.
  dumpn("Expecting exception:");
  threw = false;
  try {
    dispatch(selectSnapshotForDiffing(getState().snapshots[2]));
  } catch (error) {
    threw = true;
  }
  ok(threw, "Can't select more than two snapshots for diffing");

  heapWorker.destroy();
  yield front.detach();
});
