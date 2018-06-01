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
const { setSelectedRecording } = require("devtools/client/performance/test/helpers/recording-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const {
    EVENTS,
    $,
    PerformanceController,
    PerformanceView,
  } = panel.panelWin;

  const MAIN_CONTAINER = $("#performance-view");
  const CONTENT = $("#performance-view-content");
  const DETAILS_CONTAINER = $("#details-pane-container");
  const RECORDING = $("#recording-notice");
  const DETAILS = $("#details-pane");

  await startRecording(panel);
  await stopRecording(panel);

  await startRecording(panel);

  is(PerformanceView.getState(), "recording", "Correct state during recording.");
  is(MAIN_CONTAINER.selectedPanel, CONTENT, "Showing main view with timeline.");
  is(DETAILS_CONTAINER.selectedPanel, RECORDING, "Showing recording panel.");

  let selected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  setSelectedRecording(panel, 0);
  await selected;

  is(PerformanceView.getState(), "recorded",
     "Correct state during recording but selecting a completed recording.");
  is(MAIN_CONTAINER.selectedPanel, CONTENT, "Showing main view with timeline.");
  is(DETAILS_CONTAINER.selectedPanel, DETAILS, "Showing recorded panel.");

  selected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  setSelectedRecording(panel, 1);
  await selected;

  is(PerformanceView.getState(), "recording",
     "Correct state when switching back to recording in progress.");
  is(MAIN_CONTAINER.selectedPanel, CONTENT, "Showing main view with timeline.");
  is(DETAILS_CONTAINER.selectedPanel, RECORDING, "Showing recording panel.");

  await stopRecording(panel);

  await teardownToolboxAndRemoveTab(panel);
});
