/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler is populated by in-progress console recordings
 * when it is opened.
 */

var WAIT_TIME = 10;

function* spawnTest() {
  let { target, toolbox, console } = yield initConsole(SIMPLE_URL);
  let front = toolbox.performance;

  let profileStart = once(front, "recording-started");
  console.profile("rust");
  yield profileStart;
  profileStart = once(front, "recording-started");
  console.profile("rust2");
  yield profileStart;

  yield gDevTools.showToolbox(target, "performance");
  let panel = toolbox.getCurrentPanel();
  let { panelWin: { PerformanceController, RecordingsView }} = panel;

  yield waitUntil(() => PerformanceController.getRecordings().length === 2);
  let recordings = PerformanceController.getRecordings();
  is(recordings.length, 2, "two recordings found in the performance panel.");
  is(recordings[0].isConsole(), true, "recording came from console.profile (1).");
  is(recordings[0].getLabel(), "rust", "correct label in the recording model (1).");
  is(recordings[0].isRecording(), true, "recording is still recording (1).");
  is(recordings[1].isConsole(), true, "recording came from console.profile (2).");
  is(recordings[1].getLabel(), "rust2", "correct label in the recording model (2).");
  is(recordings[1].isRecording(), true, "recording is still recording (2).");

  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The first console recording should be selected.");

  let profileEnd = once(front, "recording-stopped");
  console.profileEnd("rust");
  yield profileEnd;
  profileEnd = once(front, "recording-stopped");
  console.profileEnd("rust2");
  yield profileEnd;

  yield teardown(panel);
  finish();
}
