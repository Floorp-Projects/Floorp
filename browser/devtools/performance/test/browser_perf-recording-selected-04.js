/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that all components can get rerendered for a profile when switching.
 */

var test = Task.async(function*() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let { $, EVENTS, PerformanceController, DetailsView, DetailsSubview, RecordingsView } = panel.panelWin;

  // Enable allocations to test the memory-calltree and memory-flamegraph.
  Services.prefs.setBoolPref(MEMORY_PREF, true);
  Services.prefs.setBoolPref(ALLOCATIONS_PREF, true);

  // Need to allow widgets to be updated while hidden, otherwise we can't use
  // `waitForWidgetsRendered`.
  DetailsSubview.canUpdateWhileHidden = true;

  yield startRecording(panel);
  yield stopRecording(panel);

  // Cycle through all the views to initialize them, otherwise we can't use
  // `waitForWidgetsRendered`. The waterfall is shown by default, but all the
  // other views are created lazily, so won't emit any events.
  yield DetailsView.selectView("js-calltree");
  yield DetailsView.selectView("js-flamegraph");
  yield DetailsView.selectView("memory-calltree");
  yield DetailsView.selectView("memory-flamegraph");

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
