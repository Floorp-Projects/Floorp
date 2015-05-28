/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


/**
 * Tests that setting the `devtools.performance.profiler.` prefs propagate to the profiler actor.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { gFront } = panel.panelWin;

  Services.prefs.setIntPref(PROFILER_BUFFER_SIZE_PREF, 1000);
  Services.prefs.setIntPref(PROFILER_SAMPLE_RATE_PREF, 2);

  yield startRecording(panel);

  let { entries, interval } = yield gFront._request("profiler", "getStartOptions");

  yield stopRecording(panel);

  is(entries, 1000, "profiler entries option is set on profiler");
  is(interval, 0.5, "profiler interval option is set on profiler");

  yield teardown(panel);
  finish();
}
