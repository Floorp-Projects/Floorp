/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test basic functionality of PerformanceFront with mock memory and timeline actors.
 */

let WAIT_TIME = 100;

function spawnTest () {
  let { target, front } = yield initBackend(SIMPLE_URL, {
    TEST_MOCK_MEMORY_ACTOR: true,
    TEST_MOCK_TIMELINE_ACTOR: true
  });
  Services.prefs.setBoolPref(MEMORY_PREF, true);

  let { memory, timeline } = front.getMocksInUse();
  ok(memory, "memory should be mocked.");
  ok(timeline, "timeline should be mocked.");

  let {
    profilerStartTime,
    timelineStartTime,
    memoryStartTime
  } = yield front.startRecording({
    withTicks: true,
    withMemory: true,
    withAllocations: true,
    allocationsSampleProbability: +Services.prefs.getCharPref(MEMORY_SAMPLE_PROB_PREF),
    allocationsMaxLogLength: Services.prefs.getIntPref(MEMORY_MAX_LOG_LEN_PREF)
  });

  ok(typeof profilerStartTime === "number",
    "The front.startRecording() emits a profiler start time.");
  ok(typeof timelineStartTime === "number",
    "The front.startRecording() emits a timeline start time.");
  ok(typeof memoryStartTime === "number",
    "The front.startRecording() emits a memory start time.");

  yield busyWait(WAIT_TIME);

  let {
    profilerEndTime,
    timelineEndTime,
    memoryEndTime
  } = yield front.stopRecording({
    withAllocations: true
  });

  ok(typeof profilerEndTime === "number",
    "The front.stopRecording() emits a profiler end time.");
  ok(typeof timelineEndTime === "number",
    "The front.stopRecording() emits a timeline end time.");
  ok(typeof memoryEndTime === "number",
    "The front.stopRecording() emits a memory end time.");

  ok(profilerEndTime > profilerStartTime,
    "The profilerEndTime is after profilerStartTime.");
  is(timelineEndTime, timelineStartTime,
    "The timelineEndTime is the same as timelineStartTime.");
  is(memoryEndTime, memoryStartTime,
    "The memoryEndTime is the same as memoryStartTime.");

  yield removeTab(target.tab);
  finish();
}
