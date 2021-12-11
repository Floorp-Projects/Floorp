/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that console recordings can overlap (not completely nested).
 */

const { Constants } = require("devtools/client/performance/modules/constants");
const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const {
  initPerformanceInTab,
  initConsoleInNewTab,
  teardownToolboxAndRemoveTab,
} = require("devtools/client/performance/test/helpers/panel-utils");
const {
  waitForRecordingStartedEvents,
  waitForRecordingStoppedEvents,
} = require("devtools/client/performance/test/helpers/actions");
const {
  times,
} = require("devtools/client/performance/test/helpers/event-utils");
const {
  getSelectedRecording,
} = require("devtools/client/performance/test/helpers/recording-utils");

add_task(async function() {
  const { target, console } = await initConsoleInNewTab({
    url: SIMPLE_URL,
    win: window,
  });

  const { panel } = await initPerformanceInTab({ tab: target.localTab });
  const { EVENTS, PerformanceController, OverviewView } = panel.panelWin;

  let started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
  });
  await console.profile("rust");
  await started;

  let recordings = PerformanceController.getRecordings();
  is(recordings.length, 1, "A recording found in the performance panel.");
  is(
    getSelectedRecording(panel),
    recordings[0],
    "The first console recording should be selected."
  );

  // Ensure overview is still rendering.
  await times(OverviewView, EVENTS.UI_OVERVIEW_RENDERED, 3, {
    expectedArgs: [Constants.FRAMERATE_GRAPH_LOW_RES_INTERVAL],
  });

  started = waitForRecordingStartedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
    // only emitted when an in-progress recording is selected
    skipWaitingForOverview: true,
    // the view state won't switch to "console-recording" unless the new
    // in-progress recording is selected, which won't happen
    skipWaitingForViewState: true,
  });
  await console.profile("golang");
  await started;

  recordings = PerformanceController.getRecordings();
  is(recordings.length, 2, "Two recordings found in the performance panel.");
  is(
    getSelectedRecording(panel),
    recordings[0],
    "The first console recording should still be selected."
  );

  // Ensure overview is still rendering.
  await times(OverviewView, EVENTS.UI_OVERVIEW_RENDERED, 3, {
    expectedArgs: [Constants.FRAMERATE_GRAPH_LOW_RES_INTERVAL],
  });

  let stopped = waitForRecordingStoppedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
  });
  await console.profileEnd("rust");
  await stopped;

  recordings = PerformanceController.getRecordings();
  is(recordings.length, 2, "Two recordings found in the performance panel.");
  is(
    getSelectedRecording(panel),
    recordings[0],
    "The first console recording should still be selected."
  );
  is(
    recordings[0].isRecording(),
    false,
    "The first console recording should no longer be recording."
  );

  stopped = waitForRecordingStoppedEvents(panel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
    // only emitted when a finished recording is selected
    skipWaitingForOverview: true,
    skipWaitingForSubview: true,
  });
  await console.profileEnd("golang");
  await stopped;

  recordings = PerformanceController.getRecordings();
  is(recordings.length, 2, "Two recordings found in the performance panel.");
  is(
    getSelectedRecording(panel),
    recordings[0],
    "The first console recording should still be selected."
  );
  is(
    recordings[1].isRecording(),
    false,
    "The second console recording should no longer be recording."
  );

  await teardownToolboxAndRemoveTab(panel);
});
