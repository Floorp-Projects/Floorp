/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that recording notices display buffer status when available,
 * and can switch between different recordings with the correct buffer information
 * displayed.
 */
function* spawnTest() {
  PMM_loadFrameScripts(gBrowser);
  // Keep it large, but still get to 1% relatively quick
  Services.prefs.setIntPref(PROFILER_BUFFER_SIZE_PREF, 1000000);
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { gFront, EVENTS, $, PerformanceController, PerformanceView, RecordingsView } = panel.panelWin;

  // Set a fast profiler-status update interval
  yield gFront.setProfilerStatusInterval(10);

  yield startRecording(panel);

  let percent = 0;
  while (percent === 0) {
    [,percent] = yield onceSpread(PerformanceView, EVENTS.UI_BUFFER_STATUS_UPDATED);
  }

  let bufferUsage = PerformanceController.getBufferUsageForRecording(PerformanceController.getCurrentRecording());
  is($("#details-pane-container").getAttribute("buffer-status"), "in-progress",
    "container has [buffer-status=in-progress]");
  ok($("#recording-notice .buffer-status-message").value.indexOf(percent + "%") !== -1,
    "buffer status text has correct percentage");

  // Start a console profile
  yield consoleProfile(panel.panelWin, "rust");

  percent = 0;
  while (percent <= (Math.floor(bufferUsage * 100))) {
    [,percent] = yield onceSpread(PerformanceView, EVENTS.UI_BUFFER_STATUS_UPDATED);
  }

  ok(percent > Math.floor(bufferUsage * 100), "buffer percentage increased in display");
  bufferUsage = PerformanceController.getBufferUsageForRecording(PerformanceController.getCurrentRecording());

  is($("#details-pane-container").getAttribute("buffer-status"), "in-progress",
    "container has [buffer-status=in-progress]");
  ok($("#recording-notice .buffer-status-message").value.indexOf(percent + "%") !== -1,
    "buffer status text has correct percentage");

  RecordingsView.selectedIndex = 1;
  percent = 0;
  while (percent === 0) {
    [,percent] = yield onceSpread(PerformanceView, EVENTS.UI_BUFFER_STATUS_UPDATED);
  }

  ok(percent < Math.floor(bufferUsage * 100), "percentage updated for newly selected recording");
  is($("#details-pane-container").getAttribute("buffer-status"), "in-progress",
    "container has [buffer-status=in-progress]");
  ok($("#console-recording-notice .buffer-status-message").value.indexOf(percent + "%") !== -1,
    "buffer status text has correct percentage for console recording");

  yield consoleProfileEnd(panel.panelWin, "rust");
  RecordingsView.selectedIndex = 0;

  percent = 0;
  while (percent <= (Math.floor(bufferUsage * 100))) {
    [,percent] = yield onceSpread(PerformanceView, EVENTS.UI_BUFFER_STATUS_UPDATED);
  }
  ok(percent > Math.floor(bufferUsage * 100), "percentage increased for original recording");
  is($("#details-pane-container").getAttribute("buffer-status"), "in-progress",
    "container has [buffer-status=in-progress]");
  ok($("#recording-notice .buffer-status-message").value.indexOf(percent + "%") !== -1,
    "buffer status text has correct percentage");

  yield stopRecording(panel);

  yield teardown(panel);
  finish();
}
