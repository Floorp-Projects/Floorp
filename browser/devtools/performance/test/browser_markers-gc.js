/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get a "GarbageCollection" marker.
 */

const TIME_CLOSE_TO = 10000;
const TEST_URL = EXAMPLE_URL + "doc_force_gc.html"

function* spawnTest () {
  let { target, front } = yield initBackend(TEST_URL);
  let markers;

  front.on("timeline-data", handler);
  let model = yield front.startRecording({ withTicks: true });

  yield waitUntil(() => {
    return !!markers;
  }, 100);

  front.off("timeline-data", handler);
  yield front.stopRecording(model);

  info(`Got ${markers.length} markers.`);

  ok(markers.every(({name}) => name === "GarbageCollection"), "All markers found are GC markers");
  ok(markers.length > 0, "found atleast one GC marker");
  ok(markers.every(({start, end}) => typeof start === "number" && start > 0 && start < end),
    "All markers have a start time between the valid range.");
  ok(markers.every(({end}) => typeof end === "number"),
    "All markers have an end time between the valid range.");
  ok(markers.every(({causeName}) => typeof causeName === "string"),
    "All markers have a causeName.");

  // Destroy the front before removing tab to ensure no
  // lingering requests
  yield front.destroy();
  yield removeTab(target.tab);
  finish();

  function handler (name, m) {
    m = m.markers;
    if (name === "markers" && m[0].name === "GarbageCollection") {
      markers = m;
    }
  }
}
