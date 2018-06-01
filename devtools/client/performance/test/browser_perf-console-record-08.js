/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler can correctly handle simultaneous console and manual
 * recordings (via `console.profile` and clicking the record button).
 */

const { Constants } = require("devtools/client/performance/modules/constants");
const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInTab, initConsoleInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { waitForRecordingStartedEvents, waitForRecordingStoppedEvents } = require("devtools/client/performance/test/helpers/actions");
const { once, times } = require("devtools/client/performance/test/helpers/event-utils");
const { setSelectedRecording } = require("devtools/client/performance/test/helpers/recording-utils");

/**
 * The following are bit flag constants that are used to represent the state of a
 * recording.
 */

// Represents a manually recorded profile, if a user hit the record button.
const MANUAL = 0;
// Represents a recorded profile from console.profile().
const CONSOLE = 1;
// Represents a profile that is currently recording.
const RECORDING = 2;
// Represents a profile that is currently selected.
const SELECTED = 4;

/**
 * Utility function to provide a meaningful inteface for testing that the bits
 * match for the recording state.
 * @param {integer} expected - The expected bit values packed in an integer.
 * @param {integer} actual - The actual bit values packed in an integer.
 */
function hasBitFlag(expected, actual) {
  return !!(expected & actual);
}

add_task(async function() {
  // This test seems to take a very long time to finish on Linux VMs.
  requestLongerTimeout(4);

  const { target, console } = await initConsoleInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { panel } = await initPerformanceInTab({ tab: target.tab });
  const { EVENTS, PerformanceController, OverviewView } = panel.panelWin;

  info("Recording 1 - Starting console.profile()...");
  let started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true
  });
  await console.profile("rust");
  await started;
  testRecordings(PerformanceController, [
    CONSOLE + SELECTED + RECORDING
  ]);

  info("Recording 2 - Starting manual recording...");
  await startRecording(panel);
  testRecordings(PerformanceController, [
    CONSOLE + RECORDING,
    MANUAL + RECORDING + SELECTED
  ]);

  info("Recording 3 - Starting console.profile(\"3\")...");
  started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
    // only emitted when an in-progress recording is selected
    skipWaitingForOverview: true,
    // the view state won't switch to "console-recording" unless the new
    // in-progress recording is selected, which won't happen
    skipWaitingForViewState: true,
  });
  await console.profile("3");
  await started;
  testRecordings(PerformanceController, [
    CONSOLE + RECORDING,
    MANUAL + RECORDING + SELECTED,
    CONSOLE + RECORDING
  ]);

  info("Recording 4 - Starting console.profile(\"4\")...");
  started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
    // only emitted when an in-progress  recording is selected
    skipWaitingForOverview: true,
    // the view state won't switch to "console-recording" unless the new
    // in-progress recording is selected, which won't happen
    skipWaitingForViewState: true,
  });
  await console.profile("4");
  await started;
  testRecordings(PerformanceController, [
    CONSOLE + RECORDING,
    MANUAL + RECORDING + SELECTED,
    CONSOLE + RECORDING,
    CONSOLE + RECORDING
  ]);

  info("Recording 4 - Ending console.profileEnd()...");
  let stopped = waitForRecordingStoppedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
    // only emitted when a finished recording is selected
    skipWaitingForOverview: true,
    skipWaitingForSubview: true,
    // the view state won't switch to "recorded" unless the new
    // finished recording is selected, which won't happen
    skipWaitingForViewState: true,
  });
  await console.profileEnd();
  await stopped;
  testRecordings(PerformanceController, [
    CONSOLE + RECORDING,
    MANUAL + RECORDING + SELECTED,
    CONSOLE + RECORDING,
    CONSOLE
  ]);

  info("Recording 4 - Select last recording...");
  let recordingSelected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  setSelectedRecording(panel, 3);
  await recordingSelected;
  testRecordings(PerformanceController, [
    CONSOLE + RECORDING,
    MANUAL + RECORDING,
    CONSOLE + RECORDING,
    CONSOLE + SELECTED
  ]);
  ok(!OverviewView.isRendering(),
    "Stop rendering overview when a completed recording is selected.");

  info("Recording 2 - Stop manual recording.");

  await stopRecording(panel);
  testRecordings(PerformanceController, [
    CONSOLE + RECORDING,
    MANUAL + SELECTED,
    CONSOLE + RECORDING,
    CONSOLE
  ]);
  ok(!OverviewView.isRendering(),
    "Stop rendering overview when a completed recording is selected.");

  info("Recording 1 - Select first recording.");
  recordingSelected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  setSelectedRecording(panel, 0);
  await recordingSelected;
  testRecordings(PerformanceController, [
    CONSOLE + RECORDING + SELECTED,
    MANUAL,
    CONSOLE + RECORDING,
    CONSOLE
  ]);
  ok(OverviewView.isRendering(),
    "Should be rendering overview a recording in progress is selected.");

  // Ensure overview is still rendering.
  await times(OverviewView, EVENTS.UI_OVERVIEW_RENDERED, 3, {
    expectedArgs: [Constants.FRAMERATE_GRAPH_LOW_RES_INTERVAL]
  });

  info("Ending console.profileEnd()...");
  stopped = waitForRecordingStoppedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
    // only emitted when a finished recording is selected
    skipWaitingForOverview: true,
    skipWaitingForSubview: true,
    // the view state won't switch to "recorded" unless the new
    // finished recording is selected, which won't happen
    skipWaitingForViewState: true,
  });
  await console.profileEnd();
  await stopped;
  testRecordings(PerformanceController, [
    CONSOLE + RECORDING + SELECTED,
    MANUAL,
    CONSOLE,
    CONSOLE
  ]);
  ok(OverviewView.isRendering(),
    "Should be rendering overview a recording in progress is selected.");

  // Ensure overview is still rendering.
  await times(OverviewView, EVENTS.UI_OVERVIEW_RENDERED, 3, {
    expectedArgs: [Constants.FRAMERATE_GRAPH_LOW_RES_INTERVAL]
  });

  info("Recording 5 - Start one more manual recording.");
  await startRecording(panel);
  testRecordings(PerformanceController, [
    CONSOLE + RECORDING,
    MANUAL,
    CONSOLE,
    CONSOLE,
    MANUAL + RECORDING + SELECTED
  ]);
  ok(OverviewView.isRendering(),
    "Should be rendering overview a recording in progress is selected.");

  // Ensure overview is still rendering.
  await times(OverviewView, EVENTS.UI_OVERVIEW_RENDERED, 3, {
    expectedArgs: [Constants.FRAMERATE_GRAPH_LOW_RES_INTERVAL]
  });

  info("Recording 5 - Stop manual recording.");
  await stopRecording(panel);
  testRecordings(PerformanceController, [
    CONSOLE + RECORDING,
    MANUAL,
    CONSOLE,
    CONSOLE,
    MANUAL + SELECTED
  ]);
  ok(!OverviewView.isRendering(),
  "Stop rendering overview when a completed recording is selected.");

  info("Recording 1 - Ending console.profileEnd()...");
  stopped = waitForRecordingStoppedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
    // only emitted when a finished recording is selected
    skipWaitingForOverview: true,
    skipWaitingForSubview: true,
    // the view state won't switch to "recorded" unless the new
    // in-progress recording is selected, which won't happen
    skipWaitingForViewState: true,
  });
  await console.profileEnd();
  await stopped;
  testRecordings(PerformanceController, [
    CONSOLE,
    MANUAL,
    CONSOLE,
    CONSOLE,
    MANUAL + SELECTED
  ]);
  ok(!OverviewView.isRendering(),
    "Stop rendering overview when a completed recording is selected.");

  await teardownToolboxAndRemoveTab(panel);
});

function testRecordings(controller, expectedBitFlags) {
  const recordings = controller.getRecordings();
  const current = controller.getCurrentRecording();
  is(recordings.length, expectedBitFlags.length, "Expected number of recordings.");

  recordings.forEach((recording, i) => {
    const expected = expectedBitFlags[i];
    is(recording.isConsole(), hasBitFlag(expected, CONSOLE),
      `Recording ${i + 1} has expected console state.`);
    is(recording.isRecording(), hasBitFlag(expected, RECORDING),
      `Recording ${i + 1} has expected console state.`);
    is((recording == current), hasBitFlag(expected, SELECTED),
      `Recording ${i + 1} has expected selected state.`);
  });
}
