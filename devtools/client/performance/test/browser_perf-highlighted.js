/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the toolbox tab for performance is highlighted when recording,
 * whether already loaded, or via console.profile with an unloaded performance tools.
 */

function* spawnTest() {
  let { target, toolbox, console } = yield initConsole(SIMPLE_URL);
  let front = toolbox.performance;
  let tab = toolbox.doc.getElementById("toolbox-tab-performance");

  let profileStart = once(front, "recording-started");
  console.profile("rust");
  yield profileStart;

  yield waitUntil(() => tab.hasAttribute("highlighted"));

  ok(tab.hasAttribute("highlighted"),
    "performance tab is highlighted during recording from console.profile when unloaded");

  let profileEnd = once(front, "recording-stopped");
  console.profileEnd("rust");
  yield profileEnd;

  ok(!tab.hasAttribute("highlighted"),
    "performance tab is no longer highlighted when console.profile recording finishes");

  yield gDevTools.showToolbox(target, "performance");
  let panel = toolbox.getCurrentPanel();
  let { panelWin: { PerformanceController, RecordingsView }} = panel;

  yield startRecording(panel);

  ok(tab.hasAttribute("highlighted"),
    "performance tab is highlighted during recording while in performance tool");

  yield stopRecording(panel);

  ok(!tab.hasAttribute("highlighted"),
    "performance tab is no longer highlighted when recording finishes");

  yield teardown(panel);
  finish();
}
