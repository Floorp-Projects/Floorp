/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that clearing recordings empties out the recordings list and toggles
 * the empty notice state.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPanelInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { getRecordingsCount } = require("devtools/client/performance/test/helpers/recording-utils");

add_task(function* () {
  let { panel } = yield initPanelInNewTab({
    tool: "performance",
    url: SIMPLE_URL,
    win: window
  });

  let { PerformanceController, PerformanceView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  is(getRecordingsCount(panel), 1,
    "The recordings list should have one recording.");
  isnot(PerformanceView.getState(), "empty",
    "PerformanceView should not be in an empty state.");
  isnot(PerformanceController.getCurrentRecording(), null,
    "There should be a current recording.");

  yield startRecording(panel);
  yield stopRecording(panel);

  is(getRecordingsCount(panel), 2,
    "The recordings list should have two recordings.");
  isnot(PerformanceView.getState(), "empty",
    "PerformanceView should not be in an empty state.");
  isnot(PerformanceController.getCurrentRecording(), null,
    "There should be a current recording.");

  yield PerformanceController.clearRecordings();

  is(getRecordingsCount(panel), 0,
    "The recordings list should be empty.");
  is(PerformanceView.getState(), "empty",
    "PerformanceView should be in an empty state.");
  is(PerformanceController.getCurrentRecording(), null,
    "There should be no current recording.");

  yield teardownToolboxAndRemoveTab(panel);
});
