/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that when setting recording features in the UI (like enabling framerate or memory),
 * if the target does not support these features, then the target's support overrides
 * the UI preferences when fetching configuration from a recording.
 */

const WAIT_TIME = 100;

let test = Task.async(function*() {
  yield testMockMemory();
  yield testMockMemoryAndTimeline();
  finish();
});

// Test mock memory
function *testMockMemory () {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL, "performance", {
    TEST_MOCK_MEMORY_ACTOR: true,
  });
  Services.prefs.setBoolPref(MEMORY_PREF, true);
  Services.prefs.setBoolPref(FRAMERATE_PREF, true);
  Services.prefs.setBoolPref(ALLOCATIONS_PREF, true);
  let { EVENTS, $, gFront, PerformanceController, PerformanceView, DetailsView, WaterfallView } = panel.panelWin;

  let { memory: memorySupport, timeline: timelineSupport } = gFront.getActorSupport();
  yield startRecording(panel, { waitForOverview: false });
  yield waitUntil(() => PerformanceController.getCurrentRecording().getTicks().length);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length);
  yield stopRecording(panel, { waitForOverview: false });

  let config = PerformanceController.getCurrentRecording().getConfiguration();
  let {
    markers, allocations, memory, ticks
  } = PerformanceController.getCurrentRecording().getAllData();

  ok(typeof config.bufferSize === "number", "sanity check, config options contains `bufferSize`.");

  is(config.withMemory, false,
    "Recording configuration set by target's support, not by UI prefs [No Memory Actor: withMemory]");
  is(config.withAllocations, false,
    "Recording configuration set by target's support, not by UI prefs [No Memory Actor: withAllocations]");

  is(config.withMarkers, true,
    "Recording configuration set by target's support, not by UI prefs [No Memory Actor: withMarkers]");
  is(config.withTicks, true,
    "Recording configuration set by target's support, not by UI prefs [No Memory Actor: withTicks]");

  ok(markers.length > 0, "markers exist.");
  ok(ticks.length > 0, "ticks exist.");
  isEmptyArray(memory, "memory");
  isEmptyArray(allocations.sites, "allocations.sites");
  isEmptyArray(allocations.timestamps, "allocations.timestamps");
  isEmptyArray(allocations.frames, "allocations.frames");

  is($("#overview-pane").hidden, false,
    "overview pane not hidden when server not supporting memory actors, yet UI prefs request them.");
  is($("#select-waterfall-view").hidden, false,
    "waterfall view button not hidden when memory mocked, and UI prefs enable them");
  is($("#select-js-calltree-view").hidden, false,
    "jscalltree view button not hidden when memory mocked, and UI prefs enable them");
  is($("#select-js-flamegraph-view").hidden, false,
    "jsflamegraph view button not hidden when memory mocked, and UI prefs enable them");
  is($("#select-memory-calltree-view").hidden, true,
    "memorycalltree view button hidden when memory mocked, and UI prefs enable them");
  is($("#select-memory-flamegraph-view").hidden, true,
    "memoryflamegraph view button hidden when memory mocked, and UI prefs enable them");

  yield gFront.destroy();
  yield teardown(panel);
}

// Test mock memory and timeline actor
function *testMockMemoryAndTimeline() {
  let { target, panel, toolbox } = yield initPerformance(SIMPLE_URL, "performance", {
    TEST_MOCK_MEMORY_ACTOR: true,
    TEST_MOCK_TIMELINE_ACTOR: true,
  });
  Services.prefs.setBoolPref(MEMORY_PREF, true);
  Services.prefs.setBoolPref(FRAMERATE_PREF, true);
  Services.prefs.setBoolPref(ALLOCATIONS_PREF, true);
  let { EVENTS, $, gFront, PerformanceController, PerformanceView, DetailsView, WaterfallView } = panel.panelWin;

  let { memory: memorySupport, timeline: timelineSupport } = gFront.getActorSupport();
  yield startRecording(panel, { waitForOverview: false });
  yield busyWait(WAIT_TIME);
  yield stopRecording(panel, { waitForOverview: false });

  let config = PerformanceController.getCurrentRecording().getConfiguration();
  let {
    markers, allocations, memory, ticks
  } = PerformanceController.getCurrentRecording().getAllData();

  ok(typeof config.bufferSize === "number", "sanity check, config options contains `bufferSize`.");

  is(config.withMemory, false,
    "Recording configuration set by target's support, not by UI prefs [No Memory/Timeline Actor: withMemory]");
  is(config.withAllocations, false,
    "Recording configuration set by target's support, not by UI prefs [No Memory/Timeline Actor: withAllocations]");

  is(config.withMarkers, false,
    "Recording configuration set by target's support, not by UI prefs [No Memory/Timeline Actor: withMarkers]");
  is(config.withTicks, false,
    "Recording configuration set by target's support, not by UI prefs [No Memory/Timeline Actor: withTicks]");
  isEmptyArray(markers, "markers");
  isEmptyArray(ticks, "ticks");
  isEmptyArray(memory, "memory");
  isEmptyArray(allocations.sites, "allocations.sites");
  isEmptyArray(allocations.timestamps, "allocations.timestamps");
  isEmptyArray(allocations.frames, "allocations.frames");

  is($("#overview-pane").hidden, true,
    "overview pane hidden when server not supporting memory/timeline actors, yet UI prefs request them.");
  is($("#select-waterfall-view").hidden, true,
    "waterfall view button hidden when memory/timeline mocked, and UI prefs enable them");
  is($("#select-js-calltree-view").hidden, false,
    "jscalltree view button not hidden when memory/timeline mocked, and UI prefs enable them");
  is($("#select-js-flamegraph-view").hidden, false,
    "jsflamegraph view button not hidden when memory/timeline mocked, and UI prefs enable them");
  is($("#select-memory-calltree-view").hidden, true,
    "memorycalltree view button hidden when memory/timeline mocked, and UI prefs enable them");
  is($("#select-memory-flamegraph-view").hidden, true,
    "memoryflamegraph view button hidden when memory/timeline mocked, and UI prefs enable them");

  yield gFront.destroy();
  yield teardown(panel);
}

function isEmptyArray (array, name) {
  ok(Array.isArray(array), `${name} is an array`);
  is(array.length, 0, `${name} is empty`);
}
