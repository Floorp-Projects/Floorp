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

  let deleted = Promise.defer();
  let deleteCount = 0;
  function onDeleted () {
    if (++deleteCount === 2) {
      deleted.resolve();
      PerformanceController.off(EVENTS.RECORDING_DELETED, onDeleted);
    }
  }

  PerformanceController.on(EVENTS.RECORDING_DELETED, onDeleted);

  let stopped = Promise.all([
    once(PerformanceController, EVENTS.RECORDING_STOPPED),
    deleted.promise
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
