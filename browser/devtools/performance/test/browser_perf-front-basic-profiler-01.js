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

  let { recordingDuration, profilerData } = yield front.stopRecording();

  ok(recordingDuration > 500, "recordingDuration exists");
  ok(profilerData, "profilerData exists");

  yield removeTab(target.tab);
  finish();

}
