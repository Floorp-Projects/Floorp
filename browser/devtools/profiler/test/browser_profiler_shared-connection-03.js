/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shared profiler connection can properly send requests.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let front = panel.panelWin.gFront;

  ok(!nsIProfilerModule.IsActive(),
    "The built-in profiler module should not have been automatically started.");

  let result = yield front._request("profiler", "startProfiler");
  is(result.started, true,
    "The request finished successfully and the profiler should've been started.");
  ok(nsIProfilerModule.IsActive(),
    "The built-in profiler module should now be active.");

  let result = yield front._request("profiler", "stopProfiler");
  is(result.started, false,
    "The request finished successfully and the profiler should've been stopped.");
  ok(!nsIProfilerModule.IsActive(),
    "The built-in profiler module should now be inactive.");

  yield teardown(panel);
  finish();
});
