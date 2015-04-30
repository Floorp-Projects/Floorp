/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test basic functionality of PerformanceFront, retrieving timeline data.
 */

function spawnTest () {
  let { target, front } = yield initBackend(SIMPLE_URL);

  let lastMemoryDelta = 0;
  let lastTickDelta = 0;

  let counters = {
    markers: [],
    memory: [],
    ticks: []
  };

  let deferreds = {
    markers: Promise.defer(),
    memory: Promise.defer(),
    ticks: Promise.defer()
  };

  front.on("timeline-data", handler);

  yield front.startRecording({ withMarkers: true, withMemory: true, withTicks: true });
  yield Promise.all(Object.keys(deferreds).map(type => deferreds[type].promise));
  yield front.stopRecording();
  front.off("timeline-data", handler);

  is(counters.markers.length, 1, "one marker event fired.");
  is(counters.memory.length, 3, "three memory events fired.");
  is(counters.ticks.length, 3, "three ticks events fired.");

  yield removeTab(target.tab);
  finish();

  function handler (_, name, ...args) {
    if (name === "markers") {
      if (counters.markers.length >= 1) { return; }
      let [markers] = args;
      ok(markers[0].start, "received atleast one marker with `start`");
      ok(markers[0].end, "received atleast one marker with `end`");
      ok(markers[0].name, "received atleast one marker with `name`");

      counters.markers.push(markers);
    }
    else if (name === "memory") {
      if (counters.memory.length >= 3) { return; }
      let [delta, measurement] = args;
      is(typeof delta, "number", "received `delta` in memory event");
      ok(delta > lastMemoryDelta, "received `delta` in memory event");
      ok(measurement.total, "received `total` in memory event");

      counters.memory.push({ delta, measurement });
      lastMemoryDelta = delta;
    }
    else if (name === "ticks") {
      if (counters.ticks.length >= 3) { return; }
      let [delta, timestamps] = args;
      ok(delta > lastTickDelta, "received `delta` in ticks event");

      // Timestamps aren't guaranteed to always contain tick events, since
      // they're dependent on the refresh driver, which may be blocked.

      counters.ticks.push({ delta, timestamps });
      lastTickDelta = delta;
    }
    else {
      throw new Error("unknown event " + name);
    }

    if (name === "markers" && counters[name].length === 1 ||
        name === "memory" && counters[name].length === 3 ||
        name === "ticks" && counters[name].length === 3) {
      deferreds[name].resolve();
    }
  };
}
