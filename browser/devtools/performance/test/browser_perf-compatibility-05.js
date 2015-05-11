/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that when using an older server (< Fx40) where the profiler actor does not
 * have the `filterable` trait, the samples are filtered by time on the client, rather
 * than the more performant platform code.
 */

const WAIT_TIME = 1000; // ms

function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { gFront: front, gTarget: target } = panel.panelWin;
  let connection = getPerformanceActorsConnection(target);

  // Explicitly override the profiler's trait `filterable`
  connection._profiler.traits.filterable = false;

  // Ugly, but we need to also not send the startTime to the server
  // so we don't filter it both on the server and client
  let request = target.client.request;
  target.client.request = (data, res) => {
    // Copy so we don't destructively change the request object, as we use
    // the startTime on this object for filtering in the callback
    let newData = merge({}, data, { startTime: void 0 });
    request.call(target.client, newData, res);
  };

  // Perform the first recording...

  let firstRecording = yield front.startRecording();
  let firstRecordingStartTime = firstRecording._profilerStartTime;
  info("Started profiling at: " + firstRecordingStartTime);

  busyWait(WAIT_TIME); // allow the profiler module to sample some cpu activity

  yield front.stopRecording(firstRecording);

  is(firstRecordingStartTime, 0,
    "The profiling start time should be 0 for the first recording.");
  ok(firstRecording.getDuration() >= WAIT_TIME,
    "The first recording duration is correct.");

  // Perform the second recording...

  let secondRecording = yield front.startRecording();
  let secondRecordingStartTime = secondRecording._profilerStartTime;
  info("Started profiling at: " + secondRecordingStartTime);

  busyWait(WAIT_TIME); // allow the profiler module to sample more cpu activity

  yield front.stopRecording(secondRecording);
  let secondRecordingProfile = secondRecording.getProfile();
  let secondRecordingSamples = secondRecordingProfile.threads[0].samples.data;

  isnot(secondRecording._profilerStartTime, 0,
    "The profiling start time should not be 0 on the second recording.");
  ok(secondRecording.getDuration() >= WAIT_TIME,
    "The second recording duration is correct.");

  const TIME_SLOT = secondRecordingProfile.threads[0].samples.schema.time;
  info("Second profile's first sample time: " + secondRecordingSamples[0][TIME_SLOT]);
  ok(secondRecordingSamples[0][TIME_SLOT] < secondRecordingStartTime,
    "The second recorded sample times were normalized.");
  ok(secondRecordingSamples[0][TIME_SLOT] > 0,
    "The second recorded sample times were normalized correctly.");
  ok(!secondRecordingSamples.find(e => e[TIME_SLOT] + secondRecordingStartTime <= firstRecording.getDuration()),
    "There should be no samples from the first recording in the second one, " +
    "even though the total number of frames did not overflow.");

  target.client.request = request;

  yield teardown(panel);
  finish();
}
