/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the retrieved profiler data samples are correctly filtered and
 * normalized before passed to consumers.
 */

const WAIT_TIME = 1000; // ms

function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let front = panel.panelWin.gFront;

  // Perform the first recording...

  let firstRecordingDataStart = yield front.startRecording();
  let firstRecordingStartTime = firstRecordingDataStart.profilerStartTime;
  info("Started profiling at: " + firstRecordingStartTime);

  busyWait(WAIT_TIME); // allow the profiler module to sample some cpu activity

  let firstRecordingDataStop = yield front.stopRecording();
  let firstRecordingFinishTime = firstRecordingDataStop.profilerEndTime;

  is(firstRecordingStartTime, 0,
    "The profiling start time should be 0 for the first recording.");
  ok(firstRecordingFinishTime - firstRecordingStartTime >= WAIT_TIME,
    "The first recording duration is correct.");
  ok(firstRecordingFinishTime >= WAIT_TIME,
    "The first recording finish time is correct.");

  // Perform the second recording...

  let secondRecordingDataStart = yield front.startRecording();
  let secondRecordingStartTime = secondRecordingDataStart.profilerStartTime;
  info("Started profiling at: " + secondRecordingStartTime);

  busyWait(WAIT_TIME); // allow the profiler module to sample more cpu activity

  let secondRecordingDataStop = yield front.stopRecording();
  let secondRecordingFinishTime = secondRecordingDataStop.profilerEndTime;
  let secondRecordingProfile = secondRecordingDataStop.profile;
  let secondRecordingSamples = secondRecordingProfile.threads[0].samples;

  isnot(secondRecordingStartTime, 0,
    "The profiling start time should not be 0 on the second recording.");
  ok(secondRecordingFinishTime - secondRecordingStartTime >= WAIT_TIME,
    "The second recording duration is correct.");

  ok(secondRecordingSamples[0].time < secondRecordingStartTime,
    "The second recorded sample times were normalized.");
  ok(secondRecordingSamples[0].time > 0,
    "The second recorded sample times were normalized correctly.");
  ok(!secondRecordingSamples.find(e => e.time + secondRecordingStartTime <= firstRecordingFinishTime),
    "There should be no samples from the first recording in the second one, " +
    "even though the total number of frames did not overflow.");

  yield teardown(panel);
  finish();
}
