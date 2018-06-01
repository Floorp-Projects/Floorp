/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the async reducer responding to the action `takeSnapshot(front)`
 */

const actions = require("devtools/client/memory/actions/snapshot");
const { snapshotState: states } = require("devtools/client/memory/constants");

add_task(async function() {
  const front = new StubbedMemoryFront();
  await front.attach();
  const store = Store();

  const unsubscribe = store.subscribe(checkState);

  let foundPendingState = false;
  let foundDoneState = false;

  function checkState() {
    const { snapshots } = store.getState();
    const lastSnapshot = snapshots[snapshots.length - 1];

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
    await waitUntilState(store, () => foundPendingState && foundDoneState);

    // reset state trackers
    foundDoneState = foundPendingState = false;
  }

  unsubscribe();
});
