/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler connection front does not activate the built-in
 * profiler module if not necessary, and doesn't deactivate it when
 * a recording is stopped.
 */

let test = Task.async(function*() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let front = panel.panelWin.gFront;
  loadFrameScripts();

  ok(!(yield PMM_isProfilerActive()),
    "The built-in profiler module should not have been automatically started.");

  let activated = front.once("profiler-activated");
  let rec = yield front.startRecording();
  yield activated;
  yield front.stopRecording(rec);
  ok((yield PMM_isProfilerActive()),
    "The built-in profiler module should still be active (1).");

  let alreadyActive = front.once("profiler-already-active");
  rec = yield front.startRecording();
  yield alreadyActive;
  yield front.stopRecording(rec);
  ok((yield PMM_isProfilerActive()),
    "The built-in profiler module should still be active (2).");

  // Manually tear down so we can check profiler status
  let tab = panel.target.tab;
  yield panel._toolbox.destroy();
  ok(!(yield PMM_isProfilerActive()),
    "The built-in profiler module should no longer be active.");
  yield removeTab(tab);
  tab = null;

  finish();
});
