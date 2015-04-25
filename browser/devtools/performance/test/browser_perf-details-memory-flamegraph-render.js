/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the memory flamegraph view renders content after recording.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView, MemoryFlameGraphView } = panel.panelWin;

  // Enable memory to test.
  Services.prefs.setBoolPref(MEMORY_PREF, true);

  yield startRecording(panel);
  yield busyWait(100);
  yield stopRecording(panel);

  let rendered = once(MemoryFlameGraphView, EVENTS.MEMORY_FLAMEGRAPH_RENDERED);
  yield DetailsView.selectView("memory-flamegraph");
  ok(DetailsView.isViewSelected(MemoryFlameGraphView), "The flamegraph is now selected.");
  yield rendered;

  ok(true, "MemoryFlameGraphView rendered after recording is stopped.");

  yield startRecording(panel);
  yield busyWait(100);

  rendered = once(MemoryFlameGraphView, EVENTS.MEMORY_FLAMEGRAPH_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  ok(true, "MemoryFlameGraphView rendered again after recording completed a second time.");

  yield teardown(panel);
  finish();
}
