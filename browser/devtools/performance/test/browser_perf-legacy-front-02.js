/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the recording model is populated correctly when using timeline
 * and memory actor mocks, and the correct views are shown.
 */

const WAIT_TIME = 1000;

var test = Task.async(function*() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL, "performance", {
    TEST_MOCK_TIMELINE_ACTOR: true,
    TEST_PERFORMANCE_LEGACY_FRONT: true,
  });
  Services.prefs.setBoolPref(MEMORY_PREF, true);
  let { EVENTS, $, gFront: front, PerformanceController, PerformanceView, DetailsView, JsCallTreeView } = panel.panelWin;

  ok(front.LEGACY_FRONT, true, "Using legacy front");
  is(front.traits.features.withMarkers, false, "traits.features.withMarkers is false.");
  is(front.traits.features.withTicks, false, "traits.features.withTicks is false.");
  is(front.traits.features.withMemory, false, "traits.features.withMemory is false.");
  is(front.traits.features.withAllocations, false, "traits.features.withAllocations is false.");

  yield startRecording(panel, { waitForOverview: false });
  busyWait(WAIT_TIME); // allow the profiler module to sample some cpu activity
  yield stopRecording(panel, { waitForOverview: false });

  let {
    label, duration, markers, frames, memory, ticks, allocations, profile
  } = PerformanceController.getCurrentRecording().getAllData();

  is(label, "", "Empty label for mock.");
  is(typeof duration, "number", "duration is a number");
  ok(duration > 0, "duration is not 0");

  isEmptyArray(markers, "markers");
  isEmptyArray(frames, "frames");
  isEmptyArray(memory, "memory");
  isEmptyArray(ticks, "ticks");
  isEmptyArray(allocations.sites, "allocations.sites");
  isEmptyArray(allocations.timestamps, "allocations.timestamps");
  isEmptyArray(allocations.frames, "allocations.frames");
  isEmptyArray(allocations.sizes, "allocations.sizes");

  let sampleCount = 0;

  for (let thread of profile.threads) {
    info("Checking thread: " + thread.name);

    for (let sample of thread.samples.data) {
      sampleCount++;

      let stack = getInflatedStackLocations(thread, sample);
      if (stack[0] != "(root)") {
        ok(false, "The sample " + stack.toSource() + " doesn't have a root node.");
      }
    }
  }

  ok(sampleCount > 0,
    "At least some samples have been iterated over, checking for root nodes.");

  is(isVisible($("#overview-pane")), false,
    "overview pane hidden when timeline mocked.");

  is($("#select-waterfall-view").hidden, true,
    "waterfall view button hidden when timeline mocked");
  is($("#select-js-calltree-view").hidden, false,
    "jscalltree view button not hidden when timeline/memory mocked");
  is($("#select-js-flamegraph-view").hidden, false,
    "jsflamegraph view button not hidden when timeline mocked");
  is($("#select-memory-calltree-view").hidden, true,
    "memorycalltree view button hidden when memory mocked");
  is($("#select-memory-flamegraph-view").hidden, true,
    "memoryflamegraph view button hidden when memory mocked");

  ok(DetailsView.isViewSelected(JsCallTreeView),
    "JS Call Tree view selected by default when timeline/memory mocked.");

  yield teardown(panel);
  finish();
});

function isEmptyArray (array, name) {
  ok(Array.isArray(array), `${name} is an array`);
  is(array.length, 0, `${name} is empty`);
}
