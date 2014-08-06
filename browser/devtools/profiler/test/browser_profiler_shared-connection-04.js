/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shared profiler connection's console notifications are
 * handled as expected.
 */

let test = Task.async(function*() {
  // This test seems to be a bit slow on debug builds.
  requestLongerTimeout(3);

  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);

  let SharedProfilerUtils = devtools.require("devtools/profiler/shared");
  let sharedProfilerConnection = SharedProfilerUtils.getProfilerConnection(panel._toolbox);

  let pendingRecordings = sharedProfilerConnection._pendingConsoleRecordings;
  let finishedRecordings = sharedProfilerConnection._finishedConsoleRecordings;
  let stackSize = 0;
  sharedProfilerConnection.on("profile", () => stackSize++);
  sharedProfilerConnection.on("profileEnd", () => stackSize--);

  ok(!(yield sharedProfilerConnection._request("profiler", "isActive")).isActive,
    "The profiler should not be active yet.");
  ok(!(yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should not be recording yet.");

  // Start calling `console.profile()`...

  let pushedLabels = [];
  pushedLabels.push((yield consoleProfile(sharedProfilerConnection)));

  // Quickly check if the profiler and framerate actor have started recording.

  ok((yield sharedProfilerConnection._request("profiler", "isActive")).isActive,
    "The profiler should have been started.");
  ok((yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should have started recording.");

  // Continue calling `console.profile()` with different arguments...

  pushedLabels.push((yield consoleProfile(sharedProfilerConnection, true, undefined)));
  pushedLabels.push((yield consoleProfile(sharedProfilerConnection, true, null)));
  pushedLabels.push((yield consoleProfile(sharedProfilerConnection, true, 0)));
  pushedLabels.push((yield consoleProfile(sharedProfilerConnection, true, "")));
  pushedLabels.push((yield consoleProfile(sharedProfilerConnection, true, [])));
  pushedLabels.push((yield consoleProfile(sharedProfilerConnection, true, [1, 2, 3])));
  pushedLabels.push((yield consoleProfile(sharedProfilerConnection, true, [4, 5, 6])));
  pushedLabels.push((yield consoleProfile(sharedProfilerConnection, true, "hello world")));
  pushedLabels.push((yield consoleProfile(sharedProfilerConnection, true, "hello world")));

  // Verify the actors' and stack state.

  ok((yield sharedProfilerConnection._request("profiler", "isActive")).isActive,
    "The profiler should still be active.");
  ok((yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should still be active.");

  is(pushedLabels.length, 10,
    "There should be 10 calls made to `console.profile()`.");
  is(pushedLabels[0], undefined,
    "The first `console.profile()` call had the correct coerced argument.");
  is(pushedLabels[1], "null",
    "The second `console.profile()` call had the correct coerced argument.");
  is(pushedLabels[2], "null",
    "The third `console.profile()` call had the correct coerced argument.");
  is(pushedLabels[3], "0",
    "The fourth `console.profile()` call had the correct coerced argument.");
  is(pushedLabels[4], "",
    "The fifth `console.profile()` call had the correct coerced argument.");
  is(pushedLabels[5], "",
    "The sixth `console.profile()` call had the correct coerced argument.");
  is(pushedLabels[6], "1,2,3",
    "The seventh `console.profile()` call had the correct coerced argument.");
  is(pushedLabels[7], "4,5,6",
    "The eigth `console.profile()` call had the correct coerced argument.");
  is(pushedLabels[8], "hello world",
    "The ninth `console.profile()` call had the correct coerced argument.");
  is(pushedLabels[9], "hello world",
    "The tenth `console.profile()` call had the correct coerced argument.");

  is(stackSize, 7,
    "There should be 7 pending profiles on the stack.");
  is(pendingRecordings.length, 7,
    "The internal pending console recordings count is correct.");
  is(finishedRecordings.length, 0,
    "The internal finished console recordings count is correct.");

  testPendingRecording(pendingRecordings, 0, pushedLabels[0]);
  testPendingRecording(pendingRecordings, 1, pushedLabels[1]);
  testPendingRecording(pendingRecordings, 2, pushedLabels[3]);
  testPendingRecording(pendingRecordings, 3, pushedLabels[4]);
  testPendingRecording(pendingRecordings, 4, pushedLabels[6]);
  testPendingRecording(pendingRecordings, 5, pushedLabels[7]);
  testPendingRecording(pendingRecordings, 6, pushedLabels[8]);

  // Start calling `console.profileEnd()`...

  let poppedLabels = [];
  poppedLabels.push((yield consoleProfileEnd(sharedProfilerConnection)));

  // Quickly check if the profiler and framerate actor are still recording.

  ok((yield sharedProfilerConnection._request("profiler", "isActive")).isActive,
    "The profiler should still be active after one recording stopped.");
  ok((yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should still be active after one recording stopped.");

  // Continue calling `console.profileEnd()` with different arguments...

  poppedLabels.push((yield consoleProfileEnd(sharedProfilerConnection, true, null)));
  poppedLabels.push((yield consoleProfileEnd(sharedProfilerConnection, true, 0)));
  poppedLabels.push((yield consoleProfileEnd(sharedProfilerConnection, true, "")));
  poppedLabels.push((yield consoleProfileEnd(sharedProfilerConnection, true, [1, 2, 3])));
  poppedLabels.push((yield consoleProfileEnd(sharedProfilerConnection, true, [4, 5, 6])));
  poppedLabels.push((yield consoleProfileEnd(sharedProfilerConnection, true, "hello world")));

  // Verify the actors' and stack state.

  ok((yield sharedProfilerConnection._request("profiler", "isActive")).isActive,
    "The profiler should still be active after all recordings stopped.");
  ok(!(yield sharedProfilerConnection._framerate.isRecording()),
    "The framerate actor should not be active after all recordings stopped.");

  is(poppedLabels.length, 7,
    "There should be 7 calls made to `console.profileEnd()`.");
  is(poppedLabels[0], undefined,
    "The first `console.profileEnd()` call had the correct coerced argument.");
  is(poppedLabels[1], "null",
    "The second `console.profileEnd()` call had the correct coerced argument.");
  is(poppedLabels[2], "0",
    "The third `console.profileEnd()` call had the correct coerced argument.");
  is(poppedLabels[3], "",
    "The fourth `console.profileEnd()` call had the correct coerced argument.");
  is(poppedLabels[4], "1,2,3",
    "The fifth `console.profileEnd()` call had the correct coerced argument.");
  is(poppedLabels[5], "4,5,6",
    "The sixth `console.profileEnd()` call had the correct coerced argument.");
  is(poppedLabels[6], "hello world",
    "The seventh `console.profileEnd()` call had the correct coerced argument.");

  is(stackSize, 0,
    "There should be 0 pending profiles on the stack.");
  is(pendingRecordings.length, 0,
    "The internal pending console recordings count is correct.");
  is(finishedRecordings.length, 7,
    "The internal finished console recordings count is correct.");

  testFinishedRecording(finishedRecordings, 0, poppedLabels[0]);
  testFinishedRecording(finishedRecordings, 1, poppedLabels[1]);
  testFinishedRecording(finishedRecordings, 2, poppedLabels[2]);
  testFinishedRecording(finishedRecordings, 3, poppedLabels[3]);
  testFinishedRecording(finishedRecordings, 4, poppedLabels[4]);
  testFinishedRecording(finishedRecordings, 5, poppedLabels[5]);
  testFinishedRecording(finishedRecordings, 6, poppedLabels[6]);

  // We're done here.

  yield teardown(panel);
  finish();
});

function* consoleProfile(connection, withLabel, labelValue) {
  let notified = connection.once("invoked-console-profile");
  if (!withLabel) {
    console.profile();
  } else {
    console.profile(labelValue);
  }
  return (yield notified);
}

function* consoleProfileEnd(connection, withLabel, labelValue) {
  let notified = connection.once("invoked-console-profileEnd");
  if (!withLabel) {
    console.profileEnd();
  } else {
    console.profileEnd(labelValue);
  }
  return (yield notified);
}

function testPendingRecording(pendingRecordings, index, label) {
  is(pendingRecordings[index].profileLabel, label,
    "The pending profile at index " + index + " on the stack has the correct label.");
  ok(pendingRecordings[index].profilingStartTime >= 0,
    "...and has a positive start time.");
}

function testFinishedRecording(finishedRecordings, index, label) {
  is(finishedRecordings[index].profilerData.profileLabel, label,
    "The finished profile at index " + index + " has the correct label.");
  ok(finishedRecordings[index].recordingDuration > 0,
    "...and has a strictly positive recording duration.");
  ok("samples" in finishedRecordings[index].profilerData.profile.threads[0],
    "...with profiler data samples attached.");
  ok("ticksData" in finishedRecordings[index],
    "...and with refresh driver ticks data attached.");
}
