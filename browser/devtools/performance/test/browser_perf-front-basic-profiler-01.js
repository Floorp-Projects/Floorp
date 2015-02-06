/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test basic functionality of PerformanceFront
 */

let WAIT_TIME = 1000;

function spawnTest () {
  let { target, front } = yield initBackend(SIMPLE_URL);

  let startData = yield front.startRecording();
  let { profilerStartTime, timelineStartTime, memoryStartTime } = startData;

  ok("profilerStartTime" in startData,
    "A `profilerStartTime` property is properly set in the recording data.");
  ok("timelineStartTime" in startData,
    "A `timelineStartTime` property is properly set in the recording data.");
  ok("memoryStartTime" in startData,
    "A `memoryStartTime` property is properly set in the recording data.");

  ok(profilerStartTime !== undefined,
    "A `profilerStartTime` property exists in the recording data.");
  ok(timelineStartTime !== undefined,
    "A `timelineStartTime` property exists in the recording data.");
  is(memoryStartTime, 0,
    "A `memoryStartTime` property exists in the recording data, but it's 0.");

  yield busyWait(WAIT_TIME);

  let stopData = yield front.stopRecording();
  let { profile, profilerEndTime, timelineEndTime, memoryEndTime } = stopData;

  ok("profile" in stopData,
    "A `profile` property is properly set in the recording data.");
  ok("profilerEndTime" in stopData,
    "A `profilerEndTime` property is properly set in the recording data.");
  ok("timelineEndTime" in stopData,
    "A `timelineEndTime` property is properly set in the recording data.");
  ok("memoryEndTime" in stopData,
    "A `memoryEndTime` property is properly set in the recording data.");

  ok(profile,
    "A `profile` property exists in the recording data.");
  ok(profilerEndTime !== undefined,
    "A `profilerEndTime` property exists in the recording data.");
  ok(timelineEndTime !== undefined,
    "A `timelineEndTime` property exists in the recording data.");
  is(memoryEndTime, 0,
    "A `memoryEndTime` property exists in the recording data, but it's 0.");

  yield removeTab(target.tab);
  finish();
}
