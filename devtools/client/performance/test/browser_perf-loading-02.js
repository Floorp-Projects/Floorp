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

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { EVENTS, $, PerformanceController } = panel.panelWin;
  const detailsContainer = $("#details-pane-container");
  const recordingNotice = $("#recording-notice");
  const loadingNotice = $("#loading-notice");
  const detailsPane = $("#details-pane");

  await startRecording(panel);

  is(detailsContainer.selectedPanel, recordingNotice,
    "The recording-notice is shown while recording.");

  let recordingStopping = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: ["recording-stopping"]
  });
  let recordingStopped = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: ["recording-stopped"]
  });
  let everythingStopped = stopRecording(panel);

  await recordingStopping;
  is(detailsContainer.selectedPanel, loadingNotice,
    "The loading-notice is shown while the record is stopping.");

  await recordingStopped;
  is(detailsContainer.selectedPanel, detailsPane,
    "The details panel is shown after the record has stopped.");

  await everythingStopped;
  await startRecording(panel);

  info("While the 2nd record is still going, switch to the first one.");
  const recordingSelected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  setSelectedRecording(panel, 0);
  await recordingSelected;

  recordingStopping = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: ["recording-stopping"]
  });
  recordingStopped = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: ["recording-stopped"]
  });
  everythingStopped = stopRecording(panel);

  await recordingStopping;
  is(detailsContainer.selectedPanel, detailsPane,
    "The details panel is still shown while the 2nd record is being stopped.");
  is(getSelectedRecordingIndex(panel), 0,
    "The first record is still selected.");

  await recordingStopped;

  is(detailsContainer.selectedPanel, detailsPane,
    "The details panel is still shown after the 2nd record has stopped.");
  is(getSelectedRecordingIndex(panel), 1,
    "The second record is now selected.");

  await everythingStopped;
  await teardownToolboxAndRemoveTab(panel);
});
