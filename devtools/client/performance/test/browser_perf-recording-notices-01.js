/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the recording notice panes are toggled in correct scenarios
 * for initialization and a single recording.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { $, PerformanceView } = panel.panelWin;

  let MAIN_CONTAINER = $("#performance-view");
  let EMPTY = $("#empty-notice");
  let CONTENT = $("#performance-view-content");
  let DETAILS_CONTAINER = $("#details-pane-container");
  let RECORDING = $("#recording-notice");
  let DETAILS = $("#details-pane");

  is(PerformanceView.getState(), "empty", "Correct default state.");
  is(MAIN_CONTAINER.selectedPanel, EMPTY, "Showing empty panel on load.");

  yield startRecording(panel);

  is(PerformanceView.getState(), "recording", "Correct state during recording.");
  is(MAIN_CONTAINER.selectedPanel, CONTENT, "Showing main view with timeline.");
  is(DETAILS_CONTAINER.selectedPanel, RECORDING, "Showing recording panel.");

  yield stopRecording(panel);

  is(PerformanceView.getState(), "recorded", "Correct state after recording.");
  is(MAIN_CONTAINER.selectedPanel, CONTENT, "Showing main view with timeline.");
  is(DETAILS_CONTAINER.selectedPanel, DETAILS, "Showing rendered graphs.");

  yield teardownToolboxAndRemoveTab(panel);
});
