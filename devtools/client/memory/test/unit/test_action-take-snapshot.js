/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the async reducer responding to the action `takeSnapshot(front)`
 */

var actions = require("devtools/client/memory/actions/snapshot");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  yield front.attach();
  let store = Store();

  let unsubscribe = store.subscribe(checkState);

  let foundPendingState = false;
  let foundDoneState = false;
  let foundAllSnapshots = false;

  function checkState () {
    let { snapshots } = store.getState();

    if (snapshots.length === 1 && snapshots[0].status === "start") {
      foundPendingState = true;
      ok(foundPendingState, "Got state change for pending heap snapshot request");
      ok(snapshots[0].selected, "First snapshot is auto-selected");
      ok(!(snapshots[0].snapshotId), "Snapshot does not yet have a snapshotId");
    }
    if (snapshots.length === 1 && snapshots[0].status === "done") {
      foundDoneState = true;
      ok(foundDoneState, "Got state change for completed heap snapshot request");
      ok(snapshots[0].snapshotId, "Snapshot fetched with a snapshotId");
    }
    if (snapshots.length === 1 && snapshots[0].status === "error") {
      ok(false, "takeSnapshot's promise returned with an error");
    }

    if (snapshots.length === 5 && snapshots.every(s => s.status === "done")) {
      foundAllSnapshots = true;
      ok(snapshots.every(s => s.status === "done"), "All snapshots have a snapshotId");
      equal(snapshots.length, 5, "Found 5 snapshots");
      ok(snapshots.every(s => s.snapshotId), "All snapshots have a snapshotId");
      ok(snapshots[0].selected, "First snapshot still selected");
      equal(snapshots.filter(s => !s.selected).length, 4, "All other snapshots are unselected");
    }
  }

  store.dispatch(actions.takeSnapshot(front));

  yield waitUntilState(store, () => foundPendingState && foundDoneState);

  for (let i = 0; i < 4; i++) {
    store.dispatch(actions.takeSnapshot(front));
  }

  yield waitUntilState(store, () => foundAllSnapshots);
  unsubscribe();
});
