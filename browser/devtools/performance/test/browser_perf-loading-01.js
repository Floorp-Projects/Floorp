/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the recordings view shows the right label while recording, after
 * recording, and once the record has loaded.
 */

let test = Task.async(function*() {
  // This test seems to take a long time to cleanup.
  requestLongerTimeout(2);

  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let { RecordingsView, PerformanceController, PerformanceView,
        EVENTS, $, L10N } = panel.panelWin;

  info("Start to record");
  yield startRecording(panel);
  let durationNode = $(".recording-item-duration",
    RecordingsView.selectedItem.target);

  is(durationNode.getAttribute("value"),
    L10N.getStr("recordingsList.recordingLabel"),
    "The duration node should show the 'recording' message while recording");

  info("Stop the recording and wait for the WILL_STOP and STOPPED events");

  let willStop = PerformanceController.once(EVENTS.RECORDING_WILL_STOP);
  let hasStopped = PerformanceController.once(EVENTS.RECORDING_STOPPED);
  let stoppingRecording = PerformanceController.stopRecording();

  yield willStop;

  is(durationNode.getAttribute("value"),
    L10N.getStr("recordingsList.loadingLabel"),
    "The duration node should show the 'loading' message while stopping");

  yield hasStopped;
  yield stoppingRecording;

  ok(PerformanceController.getCurrentRecording().isCompleted(), "recording should be completed");

  let duration = RecordingsView.selectedItem.attachment.getDuration().toFixed(0);
  is(durationNode.getAttribute("value"),
    L10N.getFormatStr("recordingsList.durationLabel", duration),
    "The duration node should show the duration after the record has stopped");

  yield PerformanceController.clearRecordings();

  yield teardown(panel);
  finish();
});
