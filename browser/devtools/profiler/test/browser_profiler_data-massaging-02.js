/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the retrieved profiler data samples are correctly filtered and
 * normalized before passed to consumers.
 */

const WAIT_TIME = 1000; // ms
const FRAMES_OVERFLOW = 1000;

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let front = panel.panelWin.gFront;

  front._customProfilerOptions = { entries: FRAMES_OVERFLOW };

  yield front.startRecording();
  let profilingStartTime = front._profilingStartTime;
  info("Started profiling at: " + profilingStartTime);

  yield idleWait(WAIT_TIME); // allow refresh driver ticks to accumulate
  busyWait(WAIT_TIME); // allow the profiler module to sample some cpu activity
  yield idleWait(WAIT_TIME); // allow refresh driver ticks to accumulate more

  let recordingData = yield front.stopRecording();
  let ticks = recordingData.ticksData;
  let profile = recordingData.profilerData.profile;
  let samples = profile.threads[0].samples;

  ok(samples[0].time >= WAIT_TIME,
    "The recorded samples overflowed, so the older ones were clamped.");

  if (ticks.length) {
    ok(ticks[0] >= samples[0].time,
      "The refresh driver ticks were filtered before being retrieved (1).");
    ok(ticks[ticks.length - 1] <= findOldestSampleTime(samples),
      "The refresh driver ticks were filtered before being retrieved (2).");
  }

  yield teardown(panel);
  finish();
});

function findOldestSampleTime(samples) {
  for (let i = samples.length - 1; i >= 0; i--) {
    let sample = samples[i];
    if ("time" in sample) {
      return sample.time;
    }
  }
}
