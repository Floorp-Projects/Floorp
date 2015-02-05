/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the recording notice panes are toggled in correct scenarios
 * for initialization and a single recording.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, PerformanceView, RecordingsView } = panel.panelWin;

  let MAIN_CONTAINER = $("#performance-view");
  let EMPTY = $("#empty-notice");
  let CONTENT = $("#performance-view-content");
  let CONTENT_CONTAINER = $("#details-pane-container");
  let RECORDING = $("#recording-notice");
  let DETAILS = $("#details-pane");

  is(PerformanceView.getState(), "empty", "correct default state");
  is(MAIN_CONTAINER.selectedPanel, EMPTY, "showing empty panel on load");

  yield startRecording(panel);

  is(PerformanceView.getState(), "recording", "correct state during recording");
  is(MAIN_CONTAINER.selectedPanel, CONTENT, "showing main view with timeline");
  is(CONTENT_CONTAINER.selectedPanel, RECORDING, "showing recording panel");

  yield stopRecording(panel);

  is(PerformanceView.getState(), "recorded", "correct state after recording");
  is(MAIN_CONTAINER.selectedPanel, CONTENT, "showing main view with timeline");
  is(CONTENT_CONTAINER.selectedPanel, DETAILS, "showing rendered graphs");

  yield teardown(panel);
  finish();
}
