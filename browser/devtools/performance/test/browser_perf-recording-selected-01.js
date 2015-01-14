/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler correctly handles multiple recordings and can
 * successfully switch between them.
 */

let test = Task.async(function*() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, RecordingsView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  yield startRecording(panel);
  yield stopRecording(panel);

  is(RecordingsView.itemCount, 2,
    "There should be two recordings visible.");
  is(RecordingsView.selectedIndex, 1,
    "The second recording item should be selected.");

  let select = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 0;
  yield select;

  is(RecordingsView.itemCount, 2,
    "There should still be two recordings visible.");
  is(RecordingsView.selectedIndex, 0,
    "The first recording item should be selected.");

  yield teardown(panel);
  finish();
});
