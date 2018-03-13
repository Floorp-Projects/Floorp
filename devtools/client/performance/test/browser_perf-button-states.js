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
  let { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { $, $$, EVENTS, PerformanceController, PerformanceView } = panel.panelWin;

  let recordButton = $("#main-record-button");

  checkRecordButtonsStates(false, false);

  let uiStartClick = once(PerformanceView, EVENTS.UI_START_RECORDING);
  let recordingStarted = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: { "1": "recording-started" }
  });
  let backendStartReady = once(PerformanceController,
                               EVENTS.BACKEND_READY_AFTER_RECORDING_START);
  let uiStateRecording = once(PerformanceView, EVENTS.UI_STATE_CHANGED, {
    expectedArgs: { "1": "recording" }
  });

  click(recordButton);
  await uiStartClick;

  checkRecordButtonsStates(true, true);

  await recordingStarted;

  checkRecordButtonsStates(true, false);

  await backendStartReady;
  await uiStateRecording;

  let uiStopClick = once(PerformanceView, EVENTS.UI_STOP_RECORDING);
  let recordingStopped = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: { "1": "recording-stopped" }
  });
  let backendStopReady = once(PerformanceController,
                               EVENTS.BACKEND_READY_AFTER_RECORDING_STOP);
  let uiStateRecorded = once(PerformanceView, EVENTS.UI_STATE_CHANGED, {
    expectedArgs: { "1": "recorded" }
  });

  click(recordButton);
  await uiStopClick;
  await recordingStopped;

  checkRecordButtonsStates(false, false);

  await backendStopReady;
  await uiStateRecorded;

  await teardownToolboxAndRemoveTab(panel);

  function checkRecordButtonsStates(checked, locked) {
    for (let button of $$(".record-button")) {
      is(button.classList.contains("checked"), checked,
         "The record button checked state should be " + checked);
      is(button.disabled, locked,
         "The record button locked state should be " + locked);
    }
  }
});
