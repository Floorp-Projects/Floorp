/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that clearing recordings empties out the recordings list and stops
 * a current recording if recording and can continue recording after.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPanelInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { times, once } = require("devtools/client/performance/test/helpers/event-utils");
const { getRecordingsCount } = require("devtools/client/performance/test/helpers/recording-utils");

add_task(async function() {
  const { panel } = await initPanelInNewTab({
    tool: "performance",
    url: SIMPLE_URL,
    win: window
  });

  const { EVENTS, PerformanceController, PerformanceView } = panel.panelWin;

  await startRecording(panel);
  await stopRecording(panel);

  is(getRecordingsCount(panel), 1,
    "The recordings list should have one recording.");
  isnot(PerformanceView.getState(), "empty",
    "PerformanceView should not be in an empty state.");
  isnot(PerformanceController.getCurrentRecording(), null,
    "There should be a current recording.");

  await startRecording(panel);

  is(getRecordingsCount(panel), 2,
    "The recordings list should have two recordings.");
  isnot(PerformanceView.getState(), "empty",
    "PerformanceView should not be in an empty state.");
  isnot(PerformanceController.getCurrentRecording(), null,
    "There should be a current recording.");

  const recordingDeleted = times(PerformanceController, EVENTS.RECORDING_DELETED, 2);
  const recordingStopped = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: ["recording-stopped"]
  });

  PerformanceController.clearRecordings();

  await recordingDeleted;
  await recordingStopped;

  is(getRecordingsCount(panel), 0,
    "The recordings list should be empty.");
  is(PerformanceView.getState(), "empty",
    "PerformanceView should be in an empty state.");
  is(PerformanceController.getCurrentRecording(), null,
    "There should be no current recording.");

  // Bug 1169146: Try another recording after clearing mid-recording.
  await startRecording(panel);
  await stopRecording(panel);

  is(getRecordingsCount(panel), 1,
    "The recordings list should have one recording.");

  await teardownToolboxAndRemoveTab(panel);
});
