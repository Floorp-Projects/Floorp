/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the details view is locked after recording has stopped and before
 * the recording has finished loading.
 * Also test that the details view isn't locked if the recording that is being
 * stopped isn't the active one.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");
const { getSelectedRecordingIndex, setSelectedRecording } = require("devtools/client/performance/test/helpers/recording-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, $, PerformanceController } = panel.panelWin;
  let detailsContainer = $("#details-pane-container");
  let recordingNotice = $("#recording-notice");
  let loadingNotice = $("#loading-notice");
  let detailsPane = $("#details-pane");

  yield startRecording(panel);

  is(detailsContainer.selectedPanel, recordingNotice,
    "The recording-notice is shown while recording.");

  let recordingStopping = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: { "1": "recording-stopping" }
  });
  let recordingStopped = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: { "1": "recording-stopped" }
  });
  let everythingStopped = stopRecording(panel);

  yield recordingStopping;
  is(detailsContainer.selectedPanel, loadingNotice,
    "The loading-notice is shown while the record is stopping.");

  yield recordingStopped;
  is(detailsContainer.selectedPanel, detailsPane,
    "The details panel is shown after the record has stopped.");

  yield everythingStopped;
  yield startRecording(panel);

  info("While the 2nd record is still going, switch to the first one.");
  let recordingSelected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  setSelectedRecording(panel, 0);
  yield recordingSelected;

  recordingStopping = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: { "1": "recording-stopping" }
  });
  recordingStopped = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: { "1": "recording-stopped" }
  });
  everythingStopped = stopRecording(panel);

  yield recordingStopping;
  is(detailsContainer.selectedPanel, detailsPane,
    "The details panel is still shown while the 2nd record is being stopped.");
  is(getSelectedRecordingIndex(panel), 0,
    "The first record is still selected.");

  yield recordingStopped;

  is(detailsContainer.selectedPanel, detailsPane,
    "The details panel is still shown after the 2nd record has stopped.");
  is(getSelectedRecordingIndex(panel), 1,
    "The second record is now selected.");

  yield everythingStopped;
  yield teardownToolboxAndRemoveTab(panel);
});
