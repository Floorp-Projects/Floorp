/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the recording notice panes are toggled when going between
 * a completed recording and an in-progress recording.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, PerformanceController, PerformanceView, RecordingsView } = panel.panelWin;

  let MAIN_CONTAINER = $("#performance-view");
  let CONTENT = $("#performance-view-content");
  let CONTENT_CONTAINER = $("#details-pane-container");
  let RECORDING = $("#recording-notice");
  let DETAILS = $("#details-pane");

  yield startRecording(panel);
  yield stopRecording(panel);

  yield startRecording(panel);

  is(PerformanceView.getState(), "recording", "correct state during recording");
  is(MAIN_CONTAINER.selectedPanel, CONTENT, "showing main view with timeline");
  is(CONTENT_CONTAINER.selectedPanel, RECORDING, "showing recording panel");

  let select = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 0;
  yield select;

  is(PerformanceView.getState(), "recorded", "correct state during recording but selecting a completed recording");
  is(MAIN_CONTAINER.selectedPanel, CONTENT, "showing main view with timeline");
  is(CONTENT_CONTAINER.selectedPanel, DETAILS, "showing recorded panel");

  select = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 1;
  yield select;

  is(PerformanceView.getState(), "recording", "correct state when switching back to recording in progress");
  is(MAIN_CONTAINER.selectedPanel, CONTENT, "showing main view with timeline");
  is(CONTENT_CONTAINER.selectedPanel, RECORDING, "showing recording panel");

  yield stopRecording(panel);

  yield teardown(panel);
  finish();
}
