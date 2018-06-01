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
const { waitForRecordingStartedEvents } = require("devtools/client/performance/test/helpers/actions");
const { idleWait } = require("devtools/client/performance/test/helpers/wait-utils");

add_task(async function() {
  const { target, console } = await initConsoleInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { panel } = await initPerformanceInTab({ tab: target.tab });
  const { PerformanceController } = panel.panelWin;

  await startRecording(panel);
  await stopRecording(panel);

  info("Starting console.profile()...");
  const started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
    // only emitted when an in-progress recording is selected
    skipWaitingForOverview: true,
    // the view state won't switch to "console-recording" unless the new
    // in-progress recording is selected, which won't happen
    skipWaitingForViewState: true,
  });
  await console.profile();
  await started;

  await PerformanceController.clearRecordings();
  let recordings = PerformanceController.getRecordings();
  is(recordings.length, 1, "One recording found in the performance panel.");
  is(recordings[0].isConsole(), true, "Recording came from console.profile.");
  is(recordings[0].getLabel(), "", "Correct label in the recording model.");
  is(PerformanceController.getCurrentRecording(), recordings[0],
    "There current recording should be the first one.");

  info("Attempting to end console.profileEnd()...");
  await console.profileEnd();
  await idleWait(1000);

  ok(true,
    "Stopping an in-progress console profile after clearing recordings does not throw.");

  await PerformanceController.clearRecordings();
  recordings = PerformanceController.getRecordings();
  is(recordings.length, 0, "No recordings found");
  is(PerformanceController.getCurrentRecording(), null,
    "There should be no current recording.");

  await teardownToolboxAndRemoveTab(panel);
});
