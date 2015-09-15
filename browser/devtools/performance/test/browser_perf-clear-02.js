/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that clearing recordings empties out the recordings list and stops
 * a current recording if recording and can continue recording after.
 */

var test = Task.async(function*() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, PerformanceView, RecordingsView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  yield startRecording(panel);

  let stopped = Promise.all([
    once(PerformanceController, EVENTS.RECORDING_STOPPED),
    once(PerformanceController, EVENTS.RECORDINGS_CLEARED)
  ]);
  PerformanceController.clearRecordings();
  yield stopped;

  is(RecordingsView.itemCount, 0,
    "RecordingsView should be empty.");
  is(PerformanceView.getState(), "empty",
    "PerformanceView should be in an empty state.");
  is(PerformanceController.getCurrentRecording(), null,
    "There should be no current recording.");

  // bug 1169146: Try another recording after clearing mid-recording
  yield startRecording(panel);
  yield stopRecording(panel);

  yield teardown(panel);
  finish();
});
