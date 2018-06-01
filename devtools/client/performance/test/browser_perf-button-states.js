/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the recording button states are set as expected.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { $, $$, EVENTS, PerformanceController, PerformanceView } = panel.panelWin;

  const recordButton = $("#main-record-button");

  checkRecordButtonsStates(false, false);

  const uiStartClick = once(PerformanceView, EVENTS.UI_START_RECORDING);
  const recordingStarted = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: ["recording-started"]
  });
  const backendStartReady = once(PerformanceController,
                               EVENTS.BACKEND_READY_AFTER_RECORDING_START);
  const uiStateRecording = once(PerformanceView, EVENTS.UI_STATE_CHANGED, {
    expectedArgs: ["recording"]
  });

  click(recordButton);
  await uiStartClick;

  checkRecordButtonsStates(true, true);

  await recordingStarted;

  checkRecordButtonsStates(true, false);

  await backendStartReady;
  await uiStateRecording;

  const uiStopClick = once(PerformanceView, EVENTS.UI_STOP_RECORDING);
  const recordingStopped = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: ["recording-stopped"]
  });
  const backendStopReady = once(PerformanceController,
                               EVENTS.BACKEND_READY_AFTER_RECORDING_STOP);
  const uiStateRecorded = once(PerformanceView, EVENTS.UI_STATE_CHANGED, {
    expectedArgs: ["recorded"]
  });

  click(recordButton);
  await uiStopClick;
  await recordingStopped;

  checkRecordButtonsStates(false, false);

  await backendStopReady;
  await uiStateRecorded;

  await teardownToolboxAndRemoveTab(panel);

  function checkRecordButtonsStates(checked, locked) {
    for (const button of $$(".record-button")) {
      is(button.classList.contains("checked"), checked,
         "The record button checked state should be " + checked);
      is(button.disabled, locked,
         "The record button locked state should be " + locked);
    }
  }
});
