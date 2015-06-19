/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that `enable-jit-optimizations` sets the recording to subsequently
 * enable the Optimizations View.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, $, DetailsView, WaterfallView, OptimizationsView } = panel.panelWin;
  Services.prefs.setBoolPref(JIT_PREF, true);


  yield startRecording(panel);
  let rendered = once(OptimizationsView, EVENTS.OPTIMIZATIONS_RENDERED);
  yield stopRecording(panel);

  yield DetailsView.selectView("optimizations");
  ok(DetailsView.isViewSelected(OptimizationsView), "The Optimizations View is now selected.");
  yield rendered;

  let recording = PerformanceController.getCurrentRecording();
  is(recording.getConfiguration().withJITOptimizations, true, "recording model has withJITOptimizations as true");

  // Set back to false, should not affect display of first recording
  info("Disabling enable-jit-optimizations");
  Services.prefs.setBoolPref(JIT_PREF, false);
  is($("#select-optimizations-view").hidden, false,
    "JIT Optimizations selector still available since the recording has it enabled.");

  yield startRecording(panel);
  rendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  yield stopRecording(panel);

  ok(DetailsView.isViewSelected(WaterfallView), "The waterfall view is now selected.");
  yield rendered;

  recording = PerformanceController.getCurrentRecording();
  is(recording.getConfiguration().withJITOptimizations, false, "recording model has withJITOptimizations as false");
  is($("#select-optimizations-view").hidden, true,
    "JIT Optimizations selector is hidden if recording did not enable optimizations.");

  yield teardown(panel);
  finish();
}
