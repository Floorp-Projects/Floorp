/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get a "GarbageCollection" marker.
 */

const TIME_CLOSE_TO = 10000;

function* spawnTest () {
  let { target, front } = yield initBackend(SIMPLE_URL);
  let markers;

  front.on("timeline-data", handler);
  let model = yield front.startRecording({ withTicks: true });

  // Check async for markers found while GC/CCing between
  yield waitUntil(() => {
    forceCC();
    return !!markers;
  }, 100);

  front.off("timeline-data", handler);
  yield front.stopRecording(model);

  info(`Got ${markers.length} markers.`);

  let maxMarkerTime = model._timelineStartTime + model.getDuration() + TIME_CLOSE_TO;

  ok(markers.every(({name}) => name === "GarbageCollection"), "All markers found are GC markers");
  ok(markers.length > 0, "found atleast one GC marker");
  ok(markers.every(({start}) => typeof start === "number" && start > 0 && start < maxMarkerTime),
    "All markers have a start time between the valid range.");
  ok(markers.every(({end}) => typeof end === "number" && end > 0 && end < maxMarkerTime),
    "All markers have an end time between the valid range.");
  ok(markers.every(({causeName}) => typeof causeName === "string"),
    "All markers have a causeName.");

  yield removeTab(target.tab);
  finish();

  function handler (_, name, m) {
    if (name === "markers" && m[0].name === "GarbageCollection") {
      markers = m;
    }
  }
}
