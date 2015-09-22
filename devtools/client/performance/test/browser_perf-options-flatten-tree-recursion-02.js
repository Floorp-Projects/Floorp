/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the memory flamegraphs get rerendered when toggling `flatten-tree-recursion`
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, DetailsView, MemoryFlameGraphView, RecordingUtils, FlameGraphUtils } = panel.panelWin;

  // Enable memory to test
  Services.prefs.setBoolPref(ALLOCATIONS_PREF, true);
  Services.prefs.setBoolPref(FLATTEN_PREF, true);

  yield startRecording(panel);
  yield busyWait(100);

  let rendered = once(MemoryFlameGraphView, EVENTS.MEMORY_FLAMEGRAPH_RENDERED);
  yield stopRecording(panel);
  yield DetailsView.selectView("memory-flamegraph");
  yield rendered;

  let allocations1 = PerformanceController.getCurrentRecording().getAllocations();
  let thread1 = RecordingUtils.getProfileThreadFromAllocations(allocations1);
  let rendering1 = FlameGraphUtils._cache.get(thread1);

  ok(allocations1,
    "The allocations were retrieved from the controller.");
  ok(thread1,
    "The allocations profile was synthesized by the utility funcs.");
  ok(rendering1,
    "The rendering data was cached.");

  rendered = once(MemoryFlameGraphView, EVENTS.MEMORY_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(FLATTEN_PREF, false);
  yield rendered;

  ok(true, "MemoryFlameGraphView rerendered when toggling flatten-tree-recursion.");

  let allocations2 = PerformanceController.getCurrentRecording().getAllocations();
  let thread2 = RecordingUtils.getProfileThreadFromAllocations(allocations2);
  let rendering2 = FlameGraphUtils._cache.get(thread2);

  is(allocations1, allocations2,
    "The same allocations data should be retrieved from the controller (1).");
  is(thread1, thread2,
    "The same allocations profile should be retrieved from the utility funcs. (1).");
  isnot(rendering1, rendering2,
    "The rendering data should be different because other options were used (1).");

  rendered = once(MemoryFlameGraphView, EVENTS.MEMORY_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(FLATTEN_PREF, true);
  yield rendered;

  ok(true, "MemoryFlameGraphView rerendered when toggling back flatten-tree-recursion.");

  let allocations3 = PerformanceController.getCurrentRecording().getAllocations();
  let thread3 = RecordingUtils.getProfileThreadFromAllocations(allocations3);
  let rendering3 = FlameGraphUtils._cache.get(thread3);

  is(allocations2, allocations3,
    "The same allocations data should be retrieved from the controller (2).");
  is(thread2, thread3,
    "The same allocations profile should be retrieved from the utility funcs. (2).");
  isnot(rendering2, rendering3,
    "The rendering data should be different because other options were used (2).");

  yield teardown(panel);
  finish();
}
