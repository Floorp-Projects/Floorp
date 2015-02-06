/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler UI does not forget that recording is active when
 * selected recording changes. Bug 1060885.
 */

let test = Task.async(function*() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let { $, EVENTS, PerformanceController, RecordingsView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  yield startRecording(panel);

  info("Selecting recording #0 and waiting for it to be displayed.");
  let select = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 0;
  yield select;

  ok($("#main-record-button").hasAttribute("checked"),
    "Button is still checked after selecting another item.");

  ok(!$("#main-record-button").hasAttribute("locked"),
    "Button is not locked after selecting another item.");

  yield stopRecording(panel);
  yield teardown(panel);
  finish();
});
