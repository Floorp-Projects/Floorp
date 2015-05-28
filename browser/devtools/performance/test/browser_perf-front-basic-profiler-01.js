/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test basic functionality of PerformanceFront
 */

let WAIT_TIME = 1000;

function* spawnTest() {
  let { target, front } = yield initBackend(SIMPLE_URL);

  let startModel = yield front.startRecording();
  let { profilerStartTime, timelineStartTime, memoryStartTime } = startModel;

  ok(startModel._profilerStartTime !== undefined,
    "A `_profilerStartTime` property exists in the recording model.");
  ok(startModel._timelineStartTime !== undefined,
    "A `_timelineStartTime` property exists in the recording model.");
  is(startModel._memoryStartTime, 0,
    "A `_memoryStartTime` property exists in the recording model, but it's 0.");

  yield busyWait(WAIT_TIME);

  let stopModel = yield front.stopRecording(startModel);

  ok(stopModel.getProfile(), "recording model has a profile after stopping.");
  ok(stopModel.getDuration(), "recording model has a duration after stopping.");

  yield removeTab(target.tab);
  finish();
}
