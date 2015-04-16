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

  let recording = yield front.startRecording({
    withTicks: true,
    withMemory: true,
    withAllocations: true,
    allocationsSampleProbability: +Services.prefs.getCharPref(MEMORY_SAMPLE_PROB_PREF),
    allocationsMaxLogLength: Services.prefs.getIntPref(MEMORY_MAX_LOG_LEN_PREF)
  });

  ok(typeof recording._profilerStartTime === "number",
    "The front.startRecording() returns a recording with a profiler start time");
  ok(typeof recording._timelineStartTime === "number",
    "The front.startRecording() returns a recording with a timeline start time");
  ok(typeof recording._memoryStartTime === "number",
    "The front.startRecording() returns a recording with a memory start time");

  yield busyWait(WAIT_TIME);

  yield front.stopRecording(recording);

  ok(typeof recording.getDuration() === "number",
    "The front.stopRecording() allows recording to get a duration.");

  ok(recording.getDuration() >= 0,
    "The profilerEndTime is after profilerStartTime.");

  yield removeTab(target.tab);
  finish();
}
