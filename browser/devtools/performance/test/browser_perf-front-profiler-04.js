/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the built-in profiler module is not reactivated if no other
 * consumer was using it over the remote debugger protocol, and ensures
 * that the actor will work properly even in such cases (e.g. the Gecko Profiler
 * addon was installed and automatically activated the profiler module).
 */

let test = Task.async(function*() {
  // Ensure the profiler is already running when the test starts.
  loadFrameScripts();
  let ENTRIES = 1000000;
  let INTERVAL = 1;
  let FEATURES = ["js"];
  yield sendProfilerCommand("StartProfiler", [ENTRIES, INTERVAL, FEATURES, FEATURES.length]);

  let { panel: firstPanel } = yield initPerformance(SIMPLE_URL);
  let firstFront = firstPanel.panelWin.gFront;

  let firstAlreadyActive = firstFront.once("profiler-already-active");
  let recording = yield firstFront.startRecording();
  yield firstAlreadyActive;
  ok(recording._profilerStartTime > 0, "The profiler was not restarted.");

  let { panel: secondPanel } = yield initPerformance(SIMPLE_URL);
  let secondFront = secondPanel.panelWin.gFront;

  let secondAlreadyActive = secondFront.once("profiler-already-active");
  let secondRecording = yield secondFront.startRecording();
  yield secondAlreadyActive;
  ok(secondRecording._profilerStartTime > 0, "The profiler was not restarted.");

  yield teardown(firstPanel);
  ok((yield PMM_isProfilerActive()),
    "The built-in profiler module should still be active.");

  yield teardown(secondPanel);
  ok(!(yield PMM_isProfilerActive()),
    "The built-in profiler module should have been automatically stoped.");

  finish();
});
