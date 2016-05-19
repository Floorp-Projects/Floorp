/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that a call to console.profileEnd() with no label ends the
 * most recent console recording, and console.profileEnd() with a label that
 * does not match any pending recordings does nothing.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInTab, initConsoleInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { waitForRecordingStartedEvents, waitForRecordingStoppedEvents } = require("devtools/client/performance/test/helpers/actions");
const { idleWait } = require("devtools/client/performance/test/helpers/wait-utils");

add_task(function* () {
  let { target, console } = yield initConsoleInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { panel } = yield initPerformanceInTab({ tab: target.tab });
  let { PerformanceController, RecordingsView } = panel.panelWin;

  let started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true
  });
  yield console.profile();
  yield started;

  started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
    // only emitted when an in-progress recording is selected
    skipWaitingForOverview: true,
    // the view state won't switch to "console-recording" unless the new
    // in-progress recording is selected, which won't happen
    skipWaitingForViewState: true,
  });
  yield console.profile("1");
  yield started;

  started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
    // only emitted when an in-progress recording is selected
    skipWaitingForOverview: true,
    // the view state won't switch to "console-recording" unless the new
    // in-progress recording is selected, which won't happen
    skipWaitingForViewState: true,
  });
  yield console.profile("2");
  yield started;

  let recordings = PerformanceController.getRecordings();
  is(recordings.length, 3, "Three recordings found in the performance panel.");
  is(recordings[0].getLabel(), "", "Checking label of recording 1");
  is(recordings[1].getLabel(), "1", "Checking label of recording 2");
  is(recordings[2].getLabel(), "2", "Checking label of recording 3");
  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The first console recording should be selected.");

  is(recordings[0].isRecording(), true,
    "All recordings should now be started. (1)");
  is(recordings[1].isRecording(), true,
    "All recordings should now be started. (2)");
  is(recordings[2].isRecording(), true,
    "All recordings should now be started. (3)");

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

  recordings = PerformanceController.getRecordings();
  is(recordings.length, 3, "Three recordings found in the performance panel.");
  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The first console recording should still be selected.");

  is(recordings[0].isRecording(), true,
    "The not most recent recording should not stop when calling console.profileEnd with no args.");
  is(recordings[1].isRecording(), true,
    "The not most recent recording should not stop when calling console.profileEnd with no args.");
  is(recordings[2].isRecording(), false,
    "Only the most recent recording should stop when calling console.profileEnd with no args.");

  info("Trying to `profileEnd` a non-existent console recording.");
  /* yield */ console.profileEnd("fxos");
  yield idleWait(1000);

  recordings = PerformanceController.getRecordings();
  is(recordings.length, 3, "Three recordings found in the performance panel.");
  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The first console recording should still be selected.");

  is(recordings[0].isRecording(), true,
    "The first recording should not be ended yet.");
  is(recordings[1].isRecording(), true,
    "The second recording should not be ended yet.");
  is(recordings[2].isRecording(), false,
    "The third recording should still be ended.");

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

  recordings = PerformanceController.getRecordings();
  is(recordings.length, 3, "Three recordings found in the performance panel.");
  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The first console recording should still be selected.");

  is(recordings[0].isRecording(), true,
    "The first recording should not be ended yet.");
  is(recordings[1].isRecording(), false,
    "The second recording should not be ended yet.");
  is(recordings[2].isRecording(), false,
    "The third recording should still be ended.");

  stopped = waitForRecordingStoppedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true
  });
  yield console.profileEnd();
  yield stopped;

  recordings = PerformanceController.getRecordings();
  is(recordings.length, 3, "Three recordings found in the performance panel.");
  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The first console recording should be selected.");

  is(recordings[0].isRecording(), false,
    "All recordings should now be ended. (1)");
  is(recordings[1].isRecording(), false,
    "All recordings should now be ended. (2)");
  is(recordings[2].isRecording(), false,
    "All recordings should now be ended. (3)");

  info("Trying to `profileEnd` with no pending recordings.");
  /* yield */ console.profileEnd();
  yield idleWait(1000);

  ok(true, "Calling console.profileEnd() with no argument and no pending recordings does not throw.");

  yield teardownToolboxAndRemoveTab(panel);
});
