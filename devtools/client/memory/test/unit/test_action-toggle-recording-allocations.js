/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the action creator `setBreakdown()` for breakdown changing.
 * Does not test refreshing the census information, check `setBreakdownAndRefresh` action
 * for that.
 */

let { toggleRecordingAllocationStacks } = require("devtools/client/memory/actions/allocations");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  const { getState, dispatch } = store;

  equal(getState().allocations.recording, false, "not recording by default");
  equal(getState().allocations.togglingInProgress, false,
        "not in the process of toggling by default");

  dispatch(toggleRecordingAllocationStacks(front));
  yield waitUntilState(store, () => getState().allocations.togglingInProgress);
  ok(true, "`togglingInProgress` set to true when toggling on");
  yield waitUntilState(store, () => !getState().allocations.togglingInProgress);

  equal(getState().allocations.recording, true, "now we are recording");
  ok(front.recordingAllocations, "front is recording too");

  dispatch(toggleRecordingAllocationStacks(front));
  yield waitUntilState(store, () => getState().allocations.togglingInProgress);
  ok(true, "`togglingInProgress` set to true when toggling off");
  yield waitUntilState(store, () => !getState().allocations.togglingInProgress);

  equal(getState().allocations.recording, false, "now we are not recording");
  ok(front.recordingAllocations, "front is not recording anymore");
});
