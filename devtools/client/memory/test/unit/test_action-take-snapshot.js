/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the async action creator `takeSnapshot(front)`
 */

var actions = require("devtools/client/memory/actions/snapshot");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  yield front.attach();
  let controller = new MemoryController({ toolbox: {}, target: {}, front });

  let unsubscribe = controller.subscribe(checkState);

  let foundPendingState = false;
  let foundDoneState = false;

  function checkState () {
    let state = controller.getState();
    if (state.snapshots.length === 1 && state.snapshots[0].status === "start") {
      foundPendingState = true;
      ok(foundPendingState, "Got state change for pending heap snapshot request");
      ok(!(state.snapshots[0].snapshotId), "Snapshot does not yet have a snapshotId");
    }
    if (state.snapshots.length === 1 && state.snapshots[0].status === "done") {
      foundDoneState = true;
      ok(foundDoneState, "Got state change for completed heap snapshot request");
      ok(state.snapshots[0].snapshotId, "Snapshot fetched with a snapshotId");
    }
    if (state.snapshots.lenght === 1 && state.snapshots[0].status === "error") {
      ok(false, "takeSnapshot's promise returned with an error");
    }
  }

  controller.dispatch(actions.takeSnapshot(front));
  yield waitUntilState(controller, () => foundPendingState && foundDoneState);

  unsubscribe();
});
