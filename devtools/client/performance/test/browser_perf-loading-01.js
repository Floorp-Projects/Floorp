/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the recordings view shows the right label while recording, after
 * recording, and once the record has loaded.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, L10N, $, PerformanceController, RecordingsView } = panel.panelWin;

  yield startRecording(panel);

  let durationLabel = $(".recording-item-duration", RecordingsView.selectedItem.target);
  is(durationLabel.getAttribute("value"),
    L10N.getStr("recordingsList.recordingLabel"),
    "The duration node should show the 'recording' message while recording");

  let recordingStopping = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: { "1": "recording-stopping" }
  });
  let recordingStopped = once(PerformanceController, EVENTS.RECORDING_STATE_CHANGE, {
    expectedArgs: { "1": "recording-stopped" }
  });
  let everythingStopped = stopRecording(panel);

  yield recordingStopping;
  is(durationLabel.getAttribute("value"),
    L10N.getStr("recordingsList.loadingLabel"),
    "The duration node should show the 'loading' message while stopping");

  yield recordingStopped;
  is(durationLabel.getAttribute("value"),
    L10N.getFormatStr("recordingsList.durationLabel",
    RecordingsView.selectedItem.attachment.getDuration().toFixed(0)),
    "The duration node should show the duration after the record has stopped");

  yield everythingStopped;
  yield teardownToolboxAndRemoveTab(panel);
});
