/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

/**
 * Tests that the details view is locked after recording has stopped and before
 * the recording has finished loading.
 * Also test that the details view isn't locked if the recording that is being
 * stopped isn't the active one.
 */

var test = Task.async(function*() {
  // This test seems to take a long time to cleanup.
  requestLongerTimeout(2);

  let { panel } = yield initPerformance(SIMPLE_URL);
  let { PerformanceController, PerformanceView, RecordingsView,
        EVENTS, $ } = panel.panelWin;

  let detailsContainer = $("#details-pane-container");
  let recordingNotice = $("#recording-notice");
  let loadingNotice = $("#loading-notice");
  let detailsPane = $("#details-pane");

  info("Start to record");
  yield startRecording(panel);

  is(detailsContainer.selectedPanel, recordingNotice,
    "The recording-notice is shown while recording");

  info("Stop the recording and wait for the WILL_STOP and STOPPED events");
  let willStop = PerformanceController.once(EVENTS.RECORDING_WILL_STOP);
  let hasStopped = PerformanceController.once(EVENTS.RECORDING_STOPPED);
  let stoppingRecording = PerformanceController.stopRecording();

  yield willStop;

  is(detailsContainer.selectedPanel, loadingNotice,
    "The loading-notice is shown while the record is stopping");

  yield hasStopped;
  yield stoppingRecording;

  is(detailsContainer.selectedPanel, detailsPane,
    "The details panel is shown after the record has stopped");

  info("Start to record again");
  yield startRecording(panel);

  info("While the 2nd record is still going, switch to the first one");
  let select = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 0;
  yield select;

  info("Stop the 2nd recording and wait for the WILL_STOP and STOPPED events");
  willStop = PerformanceController.once(EVENTS.RECORDING_WILL_STOP);
  hasStopped = PerformanceController.once(EVENTS.RECORDING_STOPPED);
  stoppingRecording = PerformanceController.stopRecording();

  yield willStop;

  is(detailsContainer.selectedPanel, detailsPane,
    "The details panel is still shown while the 2nd record is being stopped");
  is(RecordingsView.selectedIndex, 0, "The first record is still selected");

  yield hasStopped;
  yield stoppingRecording;

  is(detailsContainer.selectedPanel, detailsPane,
    "The details panel is still shown after the 2nd record has stopped");
  is(RecordingsView.selectedIndex, 1, "The second record is now selected");

  yield PerformanceController.clearRecordings();
  yield teardown(panel);
  finish();
});
