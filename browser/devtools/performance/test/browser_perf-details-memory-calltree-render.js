/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the memory call tree view renders content after recording.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(ALLOCS_URL);
  let { EVENTS, $$, PerformanceController, DetailsView, MemoryCallTreeView } = panel.panelWin;

  // Enable memory to test.
  Services.prefs.setBoolPref(ALLOCATIONS_PREF, true);

  yield startRecording(panel);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getAllocations().sizes.length);
  yield stopRecording(panel);

  let rendered = once(MemoryCallTreeView, EVENTS.MEMORY_CALL_TREE_RENDERED);
  yield DetailsView.selectView("memory-calltree");
  ok(DetailsView.isViewSelected(MemoryCallTreeView), "The call tree is now selected.");
  yield rendered;

  ok(true, "MemoryCallTreeView rendered after recording is stopped.");

  ok($$("#memory-calltree-view .call-tree-item").length, "there are several allocations rendered.");

  yield startRecording(panel);
  yield busyWait(100);

  rendered = once(MemoryCallTreeView, EVENTS.MEMORY_CALL_TREE_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  ok(true, "MemoryCallTreeView rendered again after recording completed a second time.");

  yield teardown(panel);
  finish();
}
