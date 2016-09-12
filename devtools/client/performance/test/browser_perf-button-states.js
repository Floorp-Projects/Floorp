/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the recording button states are set as expected.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { $, EVENTS, PerformanceController, PerformanceView } = panel.panelWin;
  let recordButton = $("#main-record-button");

  ok(!recordButton.hasAttribute("checked"),
    "The record button should not be checked yet.");
  ok(!recordButton.hasAttribute("locked"),
    "The record button should not be locked yet.");

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
  yield uiStartClick;

  ok(recordButton.hasAttribute("checked"),
    "The record button should now be checked.");
  ok(recordButton.hasAttribute("locked"),
    "The record button should be locked.");

  yield recordingStarted;

  ok(recordButton.hasAttribute("checked"),
    "The record button should still be checked.");
  ok(!recordButton.hasAttribute("locked"),
    "The record button should not be locked.");

  yield backendStartReady;
  yield uiStateRecording;

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
  yield uiStopClick;
  yield recordingStopped;

  ok(!recordButton.hasAttribute("checked"),
    "The record button should not be checked.");
  ok(!recordButton.hasAttribute("locked"),
    "The record button should not be locked.");

  yield backendStopReady;
  yield uiStateRecorded;

  yield teardownToolboxAndRemoveTab(panel);
});
