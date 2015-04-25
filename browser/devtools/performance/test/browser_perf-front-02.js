/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that timeline and memory actors can be started multiple times to get
 * different start times for different recording sessions.
 */

let WAIT_TIME = 1000;

function spawnTest () {
  let { target, front } = yield initBackend(SIMPLE_URL);
  let config = { withMarkers: true, withMemory: true, withTicks: true };

  yield front._request("memory", "attach");

  let timelineStart1 = yield front._request("timeline", "start", config);
  let memoryStart1 = yield front._request("memory", "startRecordingAllocations");
  let timelineStart2 = yield front._request("timeline", "start", config);
  let memoryStart2 = yield front._request("memory", "startRecordingAllocations");
  let timelineStop = yield front._request("timeline", "stop");
  let memoryStop = yield front._request("memory", "stopRecordingAllocations");

  info(timelineStart1);
  info(timelineStart2);
  ok(typeof timelineStart1 === "number", "first timeline start returned a number");
  ok(typeof timelineStart2 === "number", "second timeline start returned a number");
  ok(typeof memoryStart1 === "number", "first memory start returned a number");
  ok(typeof memoryStart2 === "number", "second memory start returned a number");
  ok(timelineStart1 < timelineStart2, "can start timeline actor twice and get different start times");
  ok(memoryStart1 < memoryStart2, "can start memory actor twice and get different start times");

  yield removeTab(target.tab);
  finish();
}
