/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the retrieved profiler data samples are correctly filtered and
 * normalized before passed to consumers.
 */

"use strict";

// time in ms
const WAIT_TIME = 1000;

const { PerformanceFront } = require("devtools/shared/fronts/performance");

add_task(function* () {
  yield addTab(MAIN_DOMAIN + "doc_perf.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = PerformanceFront(client, form);
  yield front.connect();

  // Perform the first recording...

  let firstRecording = yield front.startRecording();
  let firstRecordingStartTime = firstRecording._startTime;
  info("Started profiling at: " + firstRecordingStartTime);

  // allow the profiler module to sample some cpu activity
  busyWait(WAIT_TIME);

  yield front.stopRecording(firstRecording);

  ok(firstRecording.getDuration() >= WAIT_TIME,
    "The first recording duration is correct.");

  // Perform the second recording...

  let secondRecording = yield front.startRecording();
  let secondRecordingStartTime = secondRecording._startTime;
  info("Started profiling at: " + secondRecordingStartTime);

  // allow the profiler module to sample more cpu activity
  busyWait(WAIT_TIME);

  yield front.stopRecording(secondRecording);
  let secondRecordingProfile = secondRecording.getProfile();
  let secondRecordingSamples = secondRecordingProfile.threads[0].samples.data;

  ok(secondRecording.getDuration() >= WAIT_TIME,
    "The second recording duration is correct.");

  const TIME_SLOT = secondRecordingProfile.threads[0].samples.schema.time;
  ok(secondRecordingSamples[0][TIME_SLOT] < secondRecordingStartTime,
    "The second recorded sample times were normalized.");
  ok(secondRecordingSamples[0][TIME_SLOT] > 0,
    "The second recorded sample times were normalized correctly.");
  ok(!secondRecordingSamples.find(
        e => e[TIME_SLOT] + secondRecordingStartTime <= firstRecording.getDuration()
    ),
    "There should be no samples from the first recording in the second one, " +
    "even though the total number of frames did not overflow.");

  yield front.destroy();
  yield client.close();
  gBrowser.removeCurrentTab();
});
