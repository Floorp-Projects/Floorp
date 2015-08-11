/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test basic functionality of PerformanceFront, emitting start and endtime values
 */

let WAIT_TIME = 1000;

function* spawnTest() {
  let { target, front } = yield initBackend(SIMPLE_URL);

  let recording = yield front.startRecording({
    withAllocations: true,
    allocationsSampleProbability: +Services.prefs.getCharPref(MEMORY_SAMPLE_PROB_PREF),
    allocationsMaxLogLength: Services.prefs.getIntPref(MEMORY_MAX_LOG_LEN_PREF)
  });

  let allocationsCount = 0;
  let allocationsCounter = (_, type) => type === "allocations" && allocationsCount++;

  // Record allocation events to ensure it's called more than once
  // so we know it's polling
  front.on("timeline-data", allocationsCounter);

  ok(typeof recording._profilerStartTime === "number",
    "The front.startRecording() returns a recording model with a profiler start time.");
  ok(typeof recording._timelineStartTime === "number",
    "The front.startRecording() returns a recording model with a timeline start time.");
  ok(typeof recording._memoryStartTime === "number",
    "The front.startRecording() returns a recording model with a memory start time.");

  yield Promise.all([
    busyWait(WAIT_TIME),
    waitUntil(() => allocationsCount > 1)
  ]);

  yield front.stopRecording(recording);

  front.off("timeline-data", allocationsCounter);

  ok(typeof recording.getDuration() === "number",
    "The front.stopRecording() gives the recording model a stop time and duration.");
  ok(recording.getDuration() > 0,
    "The front.stopRecording() gives a positive duration amount.");

  is((yield front._request("memory", "getState")), "detached",
    "Memory actor is detached when stopping recording with allocations.");

  // Destroy the front before removing tab to ensure no
  // lingering requests
  yield front.destroy();
  yield removeTab(target.tab);
  finish();
}
