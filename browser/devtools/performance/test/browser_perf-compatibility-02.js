/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the recording model is populated correctly when using timeline
 * and memory actor mocks, and the correct views are shown.
 */

const WAIT_TIME = 1000;

let test = Task.async(function*() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL, "performance", {
    TEST_MOCK_MEMORY_ACTOR: true,
    TEST_MOCK_TIMELINE_ACTOR: true
  });
  Services.prefs.setBoolPref(MEMORY_PREF, true);
  let { EVENTS, $, gFront, PerformanceController, PerformanceView, DetailsView, JsCallTreeView } = panel.panelWin;

  let { memory: memorySupport, timeline: timelineSupport } = gFront.getActorSupport();
  ok(!memorySupport, "memory should be mocked.");
  ok(!timelineSupport, "timeline should be mocked.");

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
  isEmptyArray(allocations.counts, "allocations.counts");

  let sampleCount = 0;

  for (let thread of profile.threads) {
    info("Checking thread: " + thread.name);

    for (let sample of thread.samples) {
      sampleCount++;

      if (sample.frames[0].location != "(root)") {
        ok(false, "The sample " + sample.toSource() + " doesn't have a root node.");
      }
    }
  }

  ok(sampleCount > 0,
    "At least some samples have been iterated over, checking for root nodes.");

  is($("#overview-pane").hidden, true,
    "overview pane hidden when timeline mocked.");

  is($("#select-waterfall-view").hidden, true,
    "waterfall view button hidden when timeline mocked");
  is($("#select-js-calltree-view").hidden, false,
    "jscalltree view button not hidden when timeline/memory mocked");
  is($("#select-js-flamegraph-view").hidden, true,
    "jsflamegraph view button hidden when timeline mocked");
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
