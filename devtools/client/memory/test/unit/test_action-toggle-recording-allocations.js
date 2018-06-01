/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test toggling the recording of allocation stacks.
 */

const { toggleRecordingAllocationStacks } = require("devtools/client/memory/actions/allocations");

add_task(async function() {
  const front = new StubbedMemoryFront();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  equal(getState().allocations.recording, false, "not recording by default");
  equal(getState().allocations.togglingInProgress, false,
        "not in the process of toggling by default");

  dispatch(toggleRecordingAllocationStacks(front));
  await waitUntilState(store, () => getState().allocations.togglingInProgress);
  ok(true, "`togglingInProgress` set to true when toggling on");
  await waitUntilState(store, () => !getState().allocations.togglingInProgress);

  equal(getState().allocations.recording, true, "now we are recording");
  ok(front.recordingAllocations, "front is recording too");

  dispatch(toggleRecordingAllocationStacks(front));
  await waitUntilState(store, () => getState().allocations.togglingInProgress);
  ok(true, "`togglingInProgress` set to true when toggling off");
  await waitUntilState(store, () => !getState().allocations.togglingInProgress);

  equal(getState().allocations.recording, false, "now we are not recording");
  ok(front.recordingAllocations, "front is not recording anymore");

  await front.detach();
});
