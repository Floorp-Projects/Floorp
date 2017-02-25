/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we have allocation data coming from the front.
 */

"use strict";

const { PerformanceFront } = require("devtools/shared/fronts/performance");

add_task(function* () {
  yield addTab(MAIN_DOMAIN + "doc_allocations.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = PerformanceFront(client, form);
  yield front.connect();

  let rec = yield front.startRecording(
    { withMarkers: true, withAllocations: true, withTicks: true });

  yield waitUntil(() => rec.getAllocations().frames.length);
  yield waitUntil(() => rec.getAllocations().timestamps.length);
  yield waitUntil(() => rec.getAllocations().sizes.length);
  yield waitUntil(() => rec.getAllocations().sites.length);

  yield front.stopRecording(rec);

  let { timestamps, sizes } = rec.getAllocations();

  is(timestamps.length, sizes.length, "we have the same amount of timestamps and sizes");
  ok(timestamps.every(time => time > 0 && typeof time === "number"),
    "all timestamps have numeric values");
  ok(sizes.every(n => n > 0 && typeof n === "number"), "all sizes are positive numbers");

  yield front.destroy();
  yield client.close();
  gBrowser.removeCurrentTab();
});
