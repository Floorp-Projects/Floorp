/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that an error is not thrown when clearing out the recordings if there's
 * an in-progress console profile and that console profiles are not cleared
 * if in progress.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInTab, initConsoleInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { waitForRecordingStartedEvents, waitForRecordingStoppedEvents } = require("devtools/client/performance/test/helpers/actions");
const { idleWait } = require("devtools/client/performance/test/helpers/wait-utils");

add_task(function* () {
  let { target, console } = yield initConsoleInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { panel } = yield initPerformanceInTab({ tab: target.tab });
  let { PerformanceController } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  info("Starting console.profile()...");
  let started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
    // only emitted when an in-progress recording is selected
    skipWaitingForOverview: true,
    // the view state won't switch to "console-recording" unless the new
    // in-progress recording is selected, which won't happen
    skipWaitingForViewState: true,
  });
  yield console.profile();
  yield started;

  yield PerformanceController.clearRecordings();
  let recordings = PerformanceController.getRecordings();
  is(recordings.length, 1, "One recording found in the performance panel.");
  is(recordings[0].isConsole(), true, "Recording came from console.profile.");
  is(recordings[0].getLabel(), "", "Correct label in the recording model.");
  is(PerformanceController.getCurrentRecording(), recordings[0],
    "There current recording should be the first one.");

  info("Attempting to end console.profileEnd()...");
  yield console.profileEnd();
  yield idleWait(1000);

  ok(true, "Stopping an in-progress console profile after clearing recordings does not throw.");

  yield PerformanceController.clearRecordings();
  recordings = PerformanceController.getRecordings();
  is(recordings.length, 0, "No recordings found");
  is(PerformanceController.getCurrentRecording(), null,
    "There should be no current recording.");

  yield teardownToolboxAndRemoveTab(panel);
});
