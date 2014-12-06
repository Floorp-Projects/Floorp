/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test basic functionality of PerformanceFront, emitting start and endtime values
 */

let WAIT = 1000;

function spawnTest () {
  let { target, front } = yield initBackend(SIMPLE_URL);

  let { startTime } = yield front.startRecording();

  ok(typeof startTime === "number", "front.startRecording() emits start time");

  yield busyWait(WAIT);

  let { endTime } = yield front.stopRecording();

  ok(typeof endTime === "number", "front.stopRecording() emits end time");
  ok(endTime > startTime, "endTime is after startTime");

  yield removeTab(target.tab);
  finish();

}
