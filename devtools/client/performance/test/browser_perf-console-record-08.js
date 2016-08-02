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

add_task(function* () {
  // This test seems to take a very long time to finish on Linux VMs.
  requestLongerTimeout(4);

  let { target, console } = yield initConsoleInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { panel } = yield initPerformanceInTab({ tab: target.tab });
  let { EVENTS, PerformanceController, RecordingsView, OverviewView } = panel.panelWin;

  info("Starting console.profile()...");
  let started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true
  });
  yield console.profile("rust");
  yield started;
  testRecordings(PerformanceController, [C + S + R]);

  info("Starting manual recording...");
  yield startRecording(panel);
  testRecordings(PerformanceController, [C + R, R + S]);

  info("Starting console.profile(\"3\")...");
  started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
    // only emitted when an in-progress recording is selected
    skipWaitingForOverview: true,
    // the view state won't switch to "console-recording" unless the new
    // in-progress recording is selected, which won't happen
    skipWaitingForViewState: true,
  });
  yield console.profile("3");
  yield started;
  testRecordings(PerformanceController, [C + R, R + S, C + R]);

  info("Starting console.profile(\"4\")...");
  started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
    // only emitted when an in-progress  recording is selected
    skipWaitingForOverview: true,
    // the view state won't switch to "console-recording" unless the new
    // in-progress recording is selected, which won't happen
    skipWaitingForViewState: true,
  });
  yield console.profile("4");
  yield started;
  testRecordings(PerformanceController, [C + R, R + S, C + R, C + R]);

  info("Ending console.profileEnd()...");
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
  yield console.profileEnd();
  yield stopped;
  testRecordings(PerformanceController, [C + R, R + S, C + R, C]);

  info("Select last recording...");
  let recordingSelected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 3;
  yield recordingSelected;
  testRecordings(PerformanceController, [C + R, R, C + R, C + S]);
  ok(!OverviewView.isRendering(),
    "Stop rendering overview when a completed recording is selected.");

  info("Stop manual recording...");
  yield stopRecording(panel);
  testRecordings(PerformanceController, [C + R, S, C + R, C]);
  ok(!OverviewView.isRendering(),
    "Stop rendering overview when a completed recording is selected.");

  info("Select first recording...");
  recordingSelected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 0;
  yield recordingSelected;
  testRecordings(PerformanceController, [C + R + S, 0, C + R, C]);
  ok(OverviewView.isRendering(),
    "Should be rendering overview a recording in progress is selected.");

  // Ensure overview is still rendering.
  yield times(OverviewView, EVENTS.UI_OVERVIEW_RENDERED, 3, {
    expectedArgs: { "1": Constants.FRAMERATE_GRAPH_LOW_RES_INTERVAL }
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
  yield console.profileEnd();
  yield stopped;
  testRecordings(PerformanceController, [C + R + S, 0, C, C]);
  ok(OverviewView.isRendering(),
    "Should be rendering overview a recording in progress is selected.");

  // Ensure overview is still rendering.
  yield times(OverviewView, EVENTS.UI_OVERVIEW_RENDERED, 3, {
    expectedArgs: { "1": Constants.FRAMERATE_GRAPH_LOW_RES_INTERVAL }
  });

  info("Start one more manual recording...");
  yield startRecording(panel);
  testRecordings(PerformanceController, [C + R, 0, C, C, R + S]);
  ok(OverviewView.isRendering(),
    "Should be rendering overview a recording in progress is selected.");

  // Ensure overview is still rendering.
  yield times(OverviewView, EVENTS.UI_OVERVIEW_RENDERED, 3, {
    expectedArgs: { "1": Constants.FRAMERATE_GRAPH_LOW_RES_INTERVAL }
  });

  info("Stop manual recording...");
  yield stopRecording(panel);
  testRecordings(PerformanceController, [C + R, 0, C, C, S]);
  ok(!OverviewView.isRendering(),
  "Stop rendering overview when a completed recording is selected.");

  info("Ending console.profileEnd()...");
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
  yield console.profileEnd();
  yield stopped;
  testRecordings(PerformanceController, [C, 0, C, C, S]);
  ok(!OverviewView.isRendering(),
    "Stop rendering overview when a completed recording is selected.");

  yield teardownToolboxAndRemoveTab(panel);
});

// is console
const C = 1;
// is recording
const R = 2;
// is selected
const S = 4;

function testRecordings(controller, expected) {
  let recordings = controller.getRecordings();
  let current = controller.getCurrentRecording();
  is(recordings.length, expected.length, "Expected number of recordings.");

  recordings.forEach((recording, i) => {
    ok(recording.isConsole() == !!(expected[i] & C),
      `Recording ${i + 1} has expected console state.`);
    ok(recording.isRecording() == !!(expected[i] & R),
      `Recording ${i + 1} has expected console state.`);
    ok((recording == current) == !!(expected[i] & S),
      `Recording ${i + 1} has expected selected state.`);
  });
}
