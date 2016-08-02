/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that recording notices display buffer status when available,
 * and can switch between different recordings with the correct buffer
 * information displayed.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { PROFILER_BUFFER_SIZE_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { pmmLoadFrameScripts, pmmStopProfiler, pmmClearFrameScripts } = require("devtools/client/performance/test/helpers/profiler-mm-utils");
const { initPerformanceInTab, initConsoleInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { waitUntil } = require("devtools/client/performance/test/helpers/wait-utils");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(function* () {
  // Make sure the profiler module is stopped so we can set a new buffer limit.
  pmmLoadFrameScripts(gBrowser);
  yield pmmStopProfiler();

  // Keep the profiler's buffer large, but still get to 1% relatively quick.
  Services.prefs.setIntPref(PROFILER_BUFFER_SIZE_PREF, 1000000);

  let { target, console } = yield initConsoleInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { panel } = yield initPerformanceInTab({ tab: target.tab });
  let {
    gFront,
    EVENTS,
    $,
    PerformanceController,
    PerformanceView,
    RecordingsView
  } = panel.panelWin;

  // Set a fast profiler-status update interval.
  yield gFront.setProfilerStatusInterval(10);

  let DETAILS_CONTAINER = $("#details-pane-container");
  let NORMAL_BUFFER_STATUS_MESSAGE = $("#recording-notice .buffer-status-message");
  let CONSOLE_BUFFER_STATUS_MESSAGE =
    $("#console-recording-notice .buffer-status-message");
  let gPercent;

  // Start a manual recording.
  yield startRecording(panel);

  yield waitUntil(function* () {
    [, gPercent] = yield once(PerformanceView,
                              EVENTS.UI_RECORDING_PROFILER_STATUS_RENDERED,
                              { spreadArgs: true });
    return gPercent > 0;
  });

  ok(true, "Buffer percentage increased in display (1).");

  let bufferUsage = PerformanceController.getBufferUsageForRecording(
    PerformanceController.getCurrentRecording());
  either(DETAILS_CONTAINER.getAttribute("buffer-status"), "in-progress", "full",
    "Container has [buffer-status=in-progress] or [buffer-status=full].");
  ok(NORMAL_BUFFER_STATUS_MESSAGE.value.indexOf(gPercent + "%") !== -1,
    "Buffer status text has correct percentage.");

  // Start a console profile.
  yield console.profile("rust");

  yield waitUntil(function* () {
    [, gPercent] = yield once(PerformanceView,
                              EVENTS.UI_RECORDING_PROFILER_STATUS_RENDERED,
                              { spreadArgs: true });
    return gPercent > Math.floor(bufferUsage * 100);
  });

  ok(true, "Buffer percentage increased in display (2).");

  bufferUsage = PerformanceController.getBufferUsageForRecording(
    PerformanceController.getCurrentRecording());
  either(DETAILS_CONTAINER.getAttribute("buffer-status"), "in-progress", "full",
    "Container has [buffer-status=in-progress] or [buffer-status=full].");
  ok(NORMAL_BUFFER_STATUS_MESSAGE.value.indexOf(gPercent + "%") !== -1,
    "Buffer status text has correct percentage.");

  // Select the console recording.
  let selected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 1;
  yield selected;

  yield waitUntil(function* () {
    [, gPercent] = yield once(PerformanceView,
                              EVENTS.UI_RECORDING_PROFILER_STATUS_RENDERED,
                              { spreadArgs: true });
    return gPercent > 0;
  });

  ok(true, "Percentage updated for newly selected recording.");

  either(DETAILS_CONTAINER.getAttribute("buffer-status"), "in-progress", "full",
    "Container has [buffer-status=in-progress] or [buffer-status=full].");
  ok(CONSOLE_BUFFER_STATUS_MESSAGE.value.indexOf(gPercent + "%") !== -1,
    "Buffer status text has correct percentage for console recording.");

  // Stop the console profile, then select the original manual recording.
  yield console.profileEnd("rust");

  selected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 0;
  yield selected;

  yield waitUntil(function* () {
    [, gPercent] = yield once(PerformanceView,
                              EVENTS.UI_RECORDING_PROFILER_STATUS_RENDERED,
                              { spreadArgs: true });
    return gPercent > Math.floor(bufferUsage * 100);
  });

  ok(true, "Buffer percentage increased in display (3).");

  either(DETAILS_CONTAINER.getAttribute("buffer-status"), "in-progress", "full",
    "Container has [buffer-status=in-progress] or [buffer-status=full].");
  ok(NORMAL_BUFFER_STATUS_MESSAGE.value.indexOf(gPercent + "%") !== -1,
    "Buffer status text has correct percentage.");

  // Stop the manual recording.
  yield stopRecording(panel);

  yield teardownToolboxAndRemoveTab(panel);

  pmmClearFrameScripts();
});
