/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler correctly handles multiple recordings and can
 * successfully switch between them.
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

  let { EVENTS, PerformanceController, RecordingsView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  yield startRecording(panel);
  yield stopRecording(panel);

  is(RecordingsView.itemCount, 2,
    "There should be two recordings visible.");
  is(RecordingsView.selectedIndex, 1,
    "The second recording item should be selected.");

  let selected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 0;
  yield selected;

  is(RecordingsView.itemCount, 2,
    "There should still be two recordings visible.");
  is(RecordingsView.selectedIndex, 0,
    "The first recording item should be selected.");

  yield teardownToolboxAndRemoveTab(panel);
});
