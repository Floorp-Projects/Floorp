/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that recording notices display buffer status when available,
 * and can switch between different recordings with the correct buffer
 * information displayed.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const {
  PROFILER_BUFFER_SIZE_PREF,
} = require("devtools/client/performance/test/helpers/prefs");
const {
  pmmLoadFrameScripts,
  pmmStopProfiler,
  pmmClearFrameScripts,
} = require("devtools/client/performance/test/helpers/profiler-mm-utils");
const {
  initPerformanceInTab,
  initConsoleInNewTab,
  teardownToolboxAndRemoveTab,
} = require("devtools/client/performance/test/helpers/panel-utils");
const {
  startRecording,
  stopRecording,
} = require("devtools/client/performance/test/helpers/actions");
const {
  waitUntil,
} = require("devtools/client/performance/test/helpers/wait-utils");
const {
  once,
} = require("devtools/client/performance/test/helpers/event-utils");
const {
  setSelectedRecording,
} = require("devtools/client/performance/test/helpers/recording-utils");

add_task(async function() {
  // Make sure the profiler module is stopped so we can set a new buffer limit.
  pmmLoadFrameScripts(gBrowser);
  await pmmStopProfiler();

  // Keep the profiler's buffer large, but still get to 1% relatively quick.
  Services.prefs.setIntPref(PROFILER_BUFFER_SIZE_PREF, 1000000);

  const { target, console } = await initConsoleInNewTab({
    url: SIMPLE_URL,
    win: window,
  });

  const { panel } = await initPerformanceInTab({ tab: target.localTab });
  const { EVENTS, $, PerformanceController, PerformanceView } = panel.panelWin;

  // Set a fast profiler-status update interval.
  const performanceFront = await panel.target.getFront("performance");
  await performanceFront.setProfilerStatusInterval(10);

  const DETAILS_CONTAINER = $("#details-pane-container");
  const NORMAL_BUFFER_STATUS_MESSAGE = $(
    "#recording-notice .buffer-status-message"
  );
  const CONSOLE_BUFFER_STATUS_MESSAGE = $(
    "#console-recording-notice .buffer-status-message"
  );
  let gPercent;

  // Start a manual recording.
  await startRecording(panel);

  await waitUntil(async function() {
    [gPercent] = await once(
      PerformanceView,
      EVENTS.UI_RECORDING_PROFILER_STATUS_RENDERED,
      { spreadArgs: true }
    );

    return gPercent > 0;
  });

  ok(true, "Buffer percentage increased in display (1).");

  let bufferUsage = PerformanceController.getBufferUsageForRecording(
    PerformanceController.getCurrentRecording()
  );

  let bufferStatus = DETAILS_CONTAINER.getAttribute("buffer-status");
  either(
    bufferStatus,
    "in-progress",
    "full",
    "Container has [buffer-status=in-progress] or [buffer-status=full]."
  );
  ok(
    NORMAL_BUFFER_STATUS_MESSAGE.value.includes(gPercent + "%"),
    "Buffer status text has correct percentage."
  );

  // Start a console profile.
  await console.profile("rust");

  await waitUntil(async function() {
    [gPercent] = await once(
      PerformanceView,
      EVENTS.UI_RECORDING_PROFILER_STATUS_RENDERED,
      { spreadArgs: true }
    );

    // In some slow environments (eg ccov) it can happen that the buffer is full
    // during the test run. In that case, the next condition can never be true
    // because bufferUsage is 1, so we introduced a special condition to account
    // for this special case.
    if (bufferStatus === "full") {
      return gPercent === 100;
    }

    return gPercent > Math.floor(bufferUsage * 100);
  });

  ok(true, "Buffer percentage increased in display (2).");

  bufferUsage = PerformanceController.getBufferUsageForRecording(
    PerformanceController.getCurrentRecording()
  );
  bufferStatus = DETAILS_CONTAINER.getAttribute("buffer-status");
  either(
    bufferStatus,
    "in-progress",
    "full",
    "Container has [buffer-status=in-progress] or [buffer-status=full]."
  );
  ok(
    NORMAL_BUFFER_STATUS_MESSAGE.value.includes(gPercent + "%"),
    "Buffer status text has correct percentage."
  );

  // Select the console recording.
  let selected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  setSelectedRecording(panel, 1);
  await selected;

  await waitUntil(async function() {
    [gPercent] = await once(
      PerformanceView,
      EVENTS.UI_RECORDING_PROFILER_STATUS_RENDERED,
      { spreadArgs: true }
    );
    return gPercent > 0;
  });

  ok(true, "Percentage updated for newly selected recording.");

  bufferStatus = DETAILS_CONTAINER.getAttribute("buffer-status");
  either(
    bufferStatus,
    "in-progress",
    "full",
    "Container has [buffer-status=in-progress] or [buffer-status=full]."
  );
  ok(
    CONSOLE_BUFFER_STATUS_MESSAGE.value.includes(gPercent + "%"),
    "Buffer status text has correct percentage for console recording."
  );

  // Stop the console profile, then select the original manual recording.
  await console.profileEnd("rust");

  selected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  setSelectedRecording(panel, 0);
  await selected;

  bufferStatus = DETAILS_CONTAINER.getAttribute("buffer-status");
  await waitUntil(async function() {
    [gPercent] = await once(
      PerformanceView,
      EVENTS.UI_RECORDING_PROFILER_STATUS_RENDERED,
      { spreadArgs: true }
    );

    // In some slow environments (eg ccov) it can happen that the buffer is full
    // during the test run. In that case, the next condition can never be true
    // because bufferUsage is 1, so we introduced a special condition to account
    // for this special case.
    if (bufferStatus === "full") {
      return gPercent === 100;
    }

    return gPercent > Math.floor(bufferUsage * 100);
  });

  ok(true, "Buffer percentage increased in display (3).");

  bufferStatus = DETAILS_CONTAINER.getAttribute("buffer-status");
  either(
    bufferStatus,
    "in-progress",
    "full",
    "Container has [buffer-status=in-progress] or [buffer-status=full]."
  );
  ok(
    NORMAL_BUFFER_STATUS_MESSAGE.value.includes(gPercent + "%"),
    "Buffer status text has correct percentage."
  );

  // Stop the manual recording.
  await stopRecording(panel);

  await teardownToolboxAndRemoveTab(panel);

  pmmClearFrameScripts();
});
