/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the recording model is populated correctly when using timeline
 * and memory actor mocks, and that the correct button/overview displays are shown.
 */

const WAIT_TIME = 1000;

let test = Task.async(function*() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL, "performance", {
    TEST_MOCK_MEMORY_ACTOR: true
  });
  Services.prefs.setBoolPref(MEMORY_PREF, true);
  let { EVENTS, $, gFront, PerformanceController, PerformanceView, DetailsView, WaterfallView } = panel.panelWin;


  let { memory: memorySupport, timeline: timelineSupport } = gFront.getActorSupport();
  ok(!memorySupport, "memory should be mocked.");
  ok(timelineSupport, "timeline should not be mocked.");

  yield startRecording(panel);
  yield busyWait(100);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getTicks().length);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMemory().length);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length);
  yield stopRecording(panel);

  let {
    label, duration, allocations, profile
  } = PerformanceController.getCurrentRecording().getAllData();

  is(label, "", "Empty label for mock.");
  is(typeof duration, "number", "duration is a number");
  ok(duration > 0, "duration is not 0");

  isEmptyArray(allocations.sites, "allocations.sites");
  isEmptyArray(allocations.timestamps, "allocations.timestamps");
  isEmptyArray(allocations.frames, "allocations.frames");
  isEmptyArray(allocations.counts, "allocations.counts");

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

  is($("#overview-pane").hidden, false,
    "overview pane not hidden when only memory mocked.");

  is($("#select-waterfall-view").hidden, false,
    "waterfall view button not hidden when memory mocked");
  is($("#select-js-calltree-view").hidden, false,
    "jscalltree view button not hidden when memory mocked");
  is($("#select-js-flamegraph-view").hidden, false,
    "jsflamegraph view button not hidden when memory mocked");
  is($("#select-memory-calltree-view").hidden, true,
    "memorycalltree view button hidden when memory mocked");
  is($("#select-memory-flamegraph-view").hidden, true,
    "memoryflamegraph view button hidden when memory mocked");

  ok(DetailsView.isViewSelected(WaterfallView),
    "Waterfall view selected by default when memory mocked.");

  yield teardown(panel);
  finish();
});

function isEmptyArray (array, name) {
  ok(Array.isArray(array), `${name} is an array`);
  is(array.length, 0, `${name} is empty`);
}
