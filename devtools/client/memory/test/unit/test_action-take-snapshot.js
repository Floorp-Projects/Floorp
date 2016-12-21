/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the async reducer responding to the action `takeSnapshot(front)`
 */

let actions = require("devtools/client/memory/actions/snapshot");
let { snapshotState: states } = require("devtools/client/memory/constants");

function run_test() {
  run_next_test();
}

add_task(function* () {
  let front = new StubbedMemoryFront();
  yield front.attach();
  let store = Store();

  let unsubscribe = store.subscribe(checkState);

  let foundPendingState = false;
  let foundDoneState = false;

  function checkState() {
    let { snapshots } = store.getState();
    let lastSnapshot = snapshots[snapshots.length - 1];

    if (lastSnapshot.state === states.SAVING) {
      foundPendingState = true;
      ok(foundPendingState, "Got state change for pending heap snapshot request");
      ok(!lastSnapshot.path, "Snapshot does not yet have a path");
      ok(!lastSnapshot.census, "Has no census data when loading");
    } else if (lastSnapshot.state === states.SAVED) {
      foundDoneState = true;
      ok(foundDoneState, "Got state change for completed heap snapshot request");
      ok(foundPendingState, "SAVED state occurs after SAVING state");
      ok(lastSnapshot.path, "Snapshot fetched with a path");
      ok(snapshots.every(s => s.selected === (s.id === lastSnapshot.id)),
        "Only recent snapshot is selected");
    }
  }

  for (let i = 0; i < 4; i++) {
    store.dispatch(actions.takeSnapshot(front));
    yield waitUntilState(store, () => foundPendingState && foundDoneState);

    // reset state trackers
    foundDoneState = foundPendingState = false;
  }

  unsubscribe();
});
