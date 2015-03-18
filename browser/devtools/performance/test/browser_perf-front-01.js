/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test basic functionality of PerformanceFront, emitting start and endtime values
 */

let WAIT_TIME = 1000;

function spawnTest () {
  let { target, front } = yield initBackend(SIMPLE_URL);

  let {
    profilerStartTime,
    timelineStartTime,
    memoryStartTime
  } = yield front.startRecording({
    withAllocations: true,
    allocationsSampleProbability: +Services.prefs.getCharPref(MEMORY_SAMPLE_PROB_PREF),
    allocationsMaxLogLength: Services.prefs.getIntPref(MEMORY_MAX_LOG_LEN_PREF)
  });

  let allocationsCount = 0;
  let allocationsCounter = () => allocationsCount++;

  // Record allocation events to ensure it's called more than once
  // so we know it's polling
  front.on("allocations", allocationsCounter);

  ok(typeof profilerStartTime === "number",
    "The front.startRecording() emits a profiler start time.");
  ok(typeof timelineStartTime === "number",
    "The front.startRecording() emits a timeline start time.");
  ok(typeof memoryStartTime === "number",
    "The front.startRecording() emits a memory start time.");

  yield Promise.all([
    busyWait(WAIT_TIME),
    waitUntil(() => allocationsCount > 1)
  ]);

  let {
    profilerEndTime,
    timelineEndTime,
    memoryEndTime
  } = yield front.stopRecording({
    withAllocations: true
  });

  front.off("allocations", allocationsCounter);

  ok(typeof profilerEndTime === "number",
    "The front.stopRecording() emits a profiler end time.");
  ok(typeof timelineEndTime === "number",
    "The front.stopRecording() emits a timeline end time.");
  ok(typeof memoryEndTime === "number",
    "The front.stopRecording() emits a memory end time.");

  ok(profilerEndTime > profilerStartTime,
    "The profilerEndTime is after profilerStartTime.");
  ok(timelineEndTime > timelineStartTime,
    "The timelineEndTime is after timelineStartTime.");
  ok(memoryEndTime > memoryStartTime,
    "The memoryEndTime is after memoryStartTime.");

  is((yield front._request("memory", "getState")), "detached",
    "Memory actor is detached when stopping recording with allocations.");

  yield removeTab(target.tab);
  finish();
}
