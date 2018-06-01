/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test functionality of real time markers.
 */

"use strict";

const { PerformanceFront } = require("devtools/shared/fronts/performance");

add_task(async function() {
  await addTab(MAIN_DOMAIN + "doc_perf.html");

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const front = PerformanceFront(client, form);
  await front.connect();

  let lastMemoryDelta = 0;
  let lastTickDelta = 0;

  const counters = {
    markers: [],
    memory: [],
    ticks: []
  };

  const deferreds = {
    markers: defer(),
    memory: defer(),
    ticks: defer()
  };

  front.on("timeline-data", handler);

  const rec = await front.startRecording(
    { withMarkers: true, withMemory: true, withTicks: true });
  await Promise.all(Object.keys(deferreds).map(type => deferreds[type].promise));
  await front.stopRecording(rec);
  front.off("timeline-data", handler);

  is(counters.markers.length, 1, "one marker event fired.");
  is(counters.memory.length, 3, "three memory events fired.");
  is(counters.ticks.length, 3, "three ticks events fired.");

  await front.destroy();
  await client.close();
  gBrowser.removeCurrentTab();

  function handler(name, data) {
    if (name === "markers") {
      if (counters.markers.length >= 1) {
        return;
      }
      ok(data.markers[0].start, "received atleast one marker with `start`");
      ok(data.markers[0].end, "received atleast one marker with `end`");
      ok(data.markers[0].name, "received atleast one marker with `name`");

      counters.markers.push(data.markers);
    } else if (name === "memory") {
      if (counters.memory.length >= 3) {
        return;
      }
      const { delta, measurement } = data;
      is(typeof delta, "number", "received `delta` in memory event");
      ok(delta > lastMemoryDelta, "received `delta` in memory event");
      ok(measurement.total, "received `total` in memory event");

      counters.memory.push({ delta, measurement });
      lastMemoryDelta = delta;
    } else if (name === "ticks") {
      if (counters.ticks.length >= 3) {
        return;
      }
      const { delta, timestamps } = data;
      ok(delta > lastTickDelta, "received `delta` in ticks event");

      // Timestamps aren't guaranteed to always contain tick events, since
      // they're dependent on the refresh driver, which may be blocked.

      counters.ticks.push({ delta, timestamps });
      lastTickDelta = delta;
    } else if (name === "frames") {
      // Nothing to do here.
    } else {
      ok(false, `Received unknown event: ${name}`);
    }

    if (name === "markers" && counters[name].length === 1 ||
        name === "memory" && counters[name].length === 3 ||
        name === "ticks" && counters[name].length === 3) {
      deferreds[name].resolve();
    }
  }
});
