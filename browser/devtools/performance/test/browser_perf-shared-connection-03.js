/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shared PerformanceActorsConnection can properly send requests.
 */

function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let front = panel.panelWin.gFront;

  loadFrameScripts();

  ok(!(yield PMM_isProfilerActive()),
    "The built-in profiler module should not have been automatically started.");

  let result = yield front._request("profiler", "startProfiler");
  is(result.started, true,
    "The request finished successfully and the profiler should've been started.");
  ok((yield PMM_isProfilerActive()),
    "The built-in profiler module should now be active.");

  result = yield front._request("profiler", "stopProfiler");
  is(result.started, false,
    "The request finished successfully and the profiler should've been stopped.");
  ok(!(yield PMM_isProfilerActive()),
    "The built-in profiler module should now be inactive.");

  yield teardown(panel);
  finish();
}
