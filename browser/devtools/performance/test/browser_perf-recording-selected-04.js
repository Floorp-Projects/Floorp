/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that all components can get rerendered for a profile when switching.
 */

let test = Task.async(function*() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let { $, EVENTS, PerformanceController, DetailsSubview, RecordingsView } = panel.panelWin;

  DetailsSubview.canUpdateWhileHidden = true;

  yield startRecording(panel);
  yield stopRecording(panel);

  yield startRecording(panel);
  yield stopRecording(panel);

  let rerender = waitForWidgetsRendered(panel);
  RecordingsView.selectedIndex = 0;
  yield rerender;

  rerender = waitForWidgetsRendered(panel);
  RecordingsView.selectedIndex = 1;
  yield rerender;

  yield teardown(panel);
  finish();
});
