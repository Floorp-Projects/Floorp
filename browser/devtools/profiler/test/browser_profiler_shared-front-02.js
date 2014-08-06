/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler connection front does not activate the built-in
 * profiler module if not necessary, and doesn't deactivate it when
 * a recording is stopped.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let front = panel.panelWin.gFront;

  ok(!nsIProfilerModule.IsActive(),
    "The built-in profiler module should not have been automatically started.");

  let activated = front.once("profiler-activated");
  yield front.startRecording();
  yield activated;
  yield front.stopRecording();
  ok(nsIProfilerModule.IsActive(),
    "The built-in profiler module should still be active (1).");

  let alreadyActive = front.once("profiler-already-active");
  yield front.startRecording();
  yield alreadyActive;
  yield front.stopRecording();
  ok(nsIProfilerModule.IsActive(),
    "The built-in profiler module should still be active (2).");

  yield teardown(panel);

  ok(!nsIProfilerModule.IsActive(),
    "The built-in profiler module should have been automatically stoped.");

  finish();
});
