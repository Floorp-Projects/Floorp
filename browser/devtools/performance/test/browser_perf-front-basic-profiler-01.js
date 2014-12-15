/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test basic functionality of PerformanceFront
 */

let WAIT = 1000;

function spawnTest () {
  let { target, front } = yield initBackend(SIMPLE_URL);

  yield front.startRecording();
  yield busyWait(WAIT);
  let { recordingDuration, profilerData, endTime } = yield front.stopRecording();

  ok(recordingDuration > 500,
    "A `recordingDuration` property exists in the recording data.");
  ok(profilerData,
    "A `profilerData` property exists in the recording data.");
  ok(endTime,
    "A `endTime` property exists in the recording data.");

  yield removeTab(target.tab);
  finish();
}
