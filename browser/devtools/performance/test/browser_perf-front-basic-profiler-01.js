/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test basic functionality of PerformanceFront
 */

let WAIT_TIME = 1000;

function spawnTest () {
  let { target, front } = yield initBackend(SIMPLE_URL);

  let { profilerStartTime, timelineStartTime } = yield front.startRecording();

  ok(profilerStartTime !== undefined,
    "A `profilerStartTime` property exists in the recording data.");
  ok(timelineStartTime !== undefined,
    "A `timelineStartTime` property exists in the recording data.");

  yield busyWait(WAIT_TIME);

  let { profile, profilerEndTime, timelineEndTime } = yield front.stopRecording();

  ok(profile,
    "A `profile` property exists in the recording data.");
  ok(profilerEndTime !== undefined,
    "A `profilerEndTime` property exists in the recording data.");
  ok(timelineEndTime !== undefined,
    "A `timelineEndTime` property exists in the recording data.");

  yield removeTab(target.tab);
  finish();
}
