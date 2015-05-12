/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the recordings view shows the right label while recording, after
 * recording, and once the record has loaded.
 */

let test = Task.async(function*() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let { RecordingsView, PerformanceController, PerformanceView,
        EVENTS, $, L10N, ViewHelpers } = panel.panelWin;

  // This should be removed with bug 1163763.
  let DBG_STRINGS_URI = "chrome://browser/locale/devtools/debugger.properties";
  let DBG_L10N = new ViewHelpers.L10N(DBG_STRINGS_URI);

  info("Start to record");
  yield startRecording(panel);
  let durationNode = $(".recording-item-duration",
    RecordingsView.selectedItem.target);

  is(durationNode.getAttribute("value"),
    L10N.getStr("recordingsList.recordingLabel"),
    "The duration node should show the 'recording' message while recording");

  info("Stop the recording and wait for the WILL_STOP and STOPPED events");
  let clicked = PerformanceView.once(EVENTS.UI_STOP_RECORDING);
  let willStop = PerformanceController.once(EVENTS.RECORDING_WILL_STOP);
  let hasStopped = PerformanceController.once(EVENTS.RECORDING_STOPPED);

  click(panel.panelWin, $("#main-record-button"));
  yield clicked;
  yield willStop;

  is(durationNode.getAttribute("value"),
    DBG_L10N.getStr("loadingText"),
    "The duration node should show the 'loading' message while stopping");

  let stateChanged = once(PerformanceView, EVENTS.UI_STATE_CHANGED);
  yield hasStopped;
  yield stateChanged;

  let duration = RecordingsView.selectedItem.attachment.getDuration().toFixed(0);
  is(durationNode.getAttribute("value"),
    L10N.getFormatStr("recordingsList.durationLabel", duration),
    "The duration node should show the duration after the record has stopped");

  yield PerformanceController.clearRecordings();
  yield teardown(panel);
  finish();
});
