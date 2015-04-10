/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler is populated by in-progress console recordings, and
 * also console recordings that have finished before it was opened.
 */

let { getPerformanceActorsConnection } = devtools.require("devtools/performance/front");
let WAIT_TIME = 10;

function spawnTest () {
  let profilerConnected = waitForProfilerConnection();
  let { target, toolbox, console } = yield initConsole(SIMPLE_URL);
  yield profilerConnected;
  let connection = getPerformanceActorsConnection(target);

  let profileStart = once(connection, "console-profile-start");
  console.profile("rust");
  yield profileStart;

  let profileEnd = once(connection, "console-profile-end");
  console.profileEnd("rust");
  yield profileEnd;

  profileStart = once(connection, "console-profile-start");
  console.profile("rust2");
  yield profileStart;

  yield gDevTools.showToolbox(target, "performance");
  let panel = toolbox.getCurrentPanel();
  let { panelWin: { PerformanceController, RecordingsView }} = panel;

  let recordings = PerformanceController.getRecordings();
  is(recordings.length, 2, "two recordings found in the performance panel.");
  is(recordings[0].isConsole(), true, "recording came from console.profile (1).");
  is(recordings[0].getLabel(), "rust", "correct label in the recording model (1).");
  is(recordings[0].isRecording(), false, "recording is still recording (1).");
  is(recordings[1].isConsole(), true, "recording came from console.profile (2).");
  is(recordings[1].getLabel(), "rust2", "correct label in the recording model (2).");
  is(recordings[1].isRecording(), true, "recording is still recording (2).");

  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The first console recording should be selected.");

  profileEnd = once(connection, "console-profile-end");
  console.profileEnd("rust2");
  yield profileEnd;

  yield teardown(panel);
  finish();
}
