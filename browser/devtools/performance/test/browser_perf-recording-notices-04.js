/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that when a recording overlaps the circular buffer, that
 * a class is assigned to the recording notices.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL, void 0, { TEST_MOCK_PROFILER_CHECK_TIMER: 10 });
  let { EVENTS, $, PerformanceController, PerformanceView } = panel.panelWin;

  // Make sure the profiler module is stopped so we can set a new buffer limit
  loadFrameScripts();
  yield PMM_stopProfiler();
  Services.prefs.setIntPref(PROFILER_BUFFER_SIZE_PREF, 1000);

  yield startRecording(panel);

  let percent = 0;
  while (percent < 100) {
    [,percent] = yield onceSpread(PerformanceView, EVENTS.UI_BUFFER_UPDATED);
  }

  let bufferUsage = PerformanceController.getCurrentRecording().getBufferUsage();
  ok(bufferUsage, 1, "Buffer is full for this recording");
  ok($("#details-pane-container").getAttribute("buffer-status"), "full",
    "container has [buffer-status=full]");

  yield stopRecording(panel);

  yield teardown(panel);
  finish();
}
