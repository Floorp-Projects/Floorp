/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler is populated by console recordings that have finished
 * before it was opened.
 */

var WAIT_TIME = 10;

function* spawnTest() {
  let { target, toolbox, console } = yield initConsole(SIMPLE_URL);
  let front = toolbox.performance;

  let profileStart = once(front, "recording-started");
  console.profile("rust");
  yield profileStart;

  busyWait(WAIT_TIME);
  let profileEnd = once(front, "recording-stopped");
  console.profileEnd("rust");
  yield profileEnd;

  yield gDevTools.showToolbox(target, "performance");
  let panel = yield toolbox.getCurrentPanel().open();
  let { panelWin: { PerformanceController, RecordingsView }} = panel;

  let recordings = PerformanceController.getRecordings();
  yield waitUntil(() => PerformanceController.getRecordings().length === 1);
  is(recordings.length, 1, "one recording found in the performance panel.");
  is(recordings[0].isConsole(), true, "recording came from console.profile.");
  is(recordings[0].getLabel(), "rust", "correct label in the recording model.");

  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The profile from console should be selected as its the only one in the RecordingsView.");

  is(RecordingsView.selectedItem.attachment.getLabel(), "rust",
    "The profile label for the first recording is correct.");

  yield teardown(panel);
  finish();
}
