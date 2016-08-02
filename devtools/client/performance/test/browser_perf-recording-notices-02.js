/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the recording notice panes are toggled when going between
 * a completed recording and an in-progress recording.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let {
    EVENTS,
    $,
    PerformanceController,
    PerformanceView,
    RecordingsView
  } = panel.panelWin;

  let MAIN_CONTAINER = $("#performance-view");
  let CONTENT = $("#performance-view-content");
  let DETAILS_CONTAINER = $("#details-pane-container");
  let RECORDING = $("#recording-notice");
  let DETAILS = $("#details-pane");

  yield startRecording(panel);
  yield stopRecording(panel);

  yield startRecording(panel);

  is(PerformanceView.getState(), "recording", "Correct state during recording.");
  is(MAIN_CONTAINER.selectedPanel, CONTENT, "Showing main view with timeline.");
  is(DETAILS_CONTAINER.selectedPanel, RECORDING, "Showing recording panel.");

  let selected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 0;
  yield selected;

  is(PerformanceView.getState(), "recorded",
     "Correct state during recording but selecting a completed recording.");
  is(MAIN_CONTAINER.selectedPanel, CONTENT, "Showing main view with timeline.");
  is(DETAILS_CONTAINER.selectedPanel, DETAILS, "Showing recorded panel.");

  selected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 1;
  yield selected;

  is(PerformanceView.getState(), "recording",
     "Correct state when switching back to recording in progress.");
  is(MAIN_CONTAINER.selectedPanel, CONTENT, "Showing main view with timeline.");
  is(DETAILS_CONTAINER.selectedPanel, RECORDING, "Showing recording panel.");

  yield stopRecording(panel);

  yield teardownToolboxAndRemoveTab(panel);
});
