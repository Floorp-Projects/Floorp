/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler is populated by console recordings that have finished
 * before it was opened.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInTab, initConsoleInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { waitUntil } = require("devtools/client/performance/test/helpers/wait-utils");
const { getSelectedRecording } = require("devtools/client/performance/test/helpers/recording-utils");

add_task(function* () {
  let { target, console } = yield initConsoleInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  yield console.profile("rust");
  yield console.profileEnd("rust");

  let { panel } = yield initPerformanceInTab({ tab: target.tab });
  let { PerformanceController, WaterfallView } = panel.panelWin;

  yield waitUntil(() => PerformanceController.getRecordings().length == 1);
  yield waitUntil(() => WaterfallView.wasRenderedAtLeastOnce);

  let recordings = PerformanceController.getRecordings();
  is(recordings.length, 1, "One recording found in the performance panel.");
  is(recordings[0].isConsole(), true, "Recording came from console.profile.");
  is(recordings[0].getLabel(), "rust", "Correct label in the recording model.");

  const selected = getSelectedRecording(panel);

  is(selected, recordings[0],
    "The profile from console should be selected as it's the only one.");
  is(selected.getLabel(), "rust",
    "The profile label for the first recording is correct.");

  yield teardownToolboxAndRemoveTab(panel);
});
