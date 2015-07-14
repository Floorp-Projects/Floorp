/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get "nsCycleCollector::Collect" and
 * "nsCycleCollector::ForgetSkippable" markers when we force cycle collection.
 */

const TEST_URL = EXAMPLE_URL + "doc_force_cc.html"

function waitForMarkerType(front, type) {
  info("Waiting for marker of type = " + type);
  const { promise, resolve } = Promise.defer();

  const handler = (_, name, markers) => {
    if (name !== "markers") {
      return;
    }

    info("Got markers: " + JSON.stringify(markers, null, 2));

    if (markers.some(m => m.name === type)) {
      ok(true, "Found marker of type = " + type);
      front.off("timeline-data", handler);
      resolve();
    }
  };
  front.on("timeline-data", handler);

  return promise;
}

function* spawnTest () {
  // This test runs very slowly on linux32 debug EC2 instances.
  requestLongerTimeout(2);

  let { target, front } = yield initBackend(TEST_URL);

  yield front.startRecording({ withMarkers: true, withTicks: true });

  yield Promise.all([
    waitForMarkerType(front, "nsCycleCollector::Collect"),
    waitForMarkerType(front, "nsCycleCollector::ForgetSkippable")
  ]);
  ok(true, "Got expected cycle collection events");

  yield front.stopRecording();

  // Destroy the front before removing tab to ensure no
  // lingering requests
  yield front.destroy();
  yield removeTab(target.tab);
  finish();
}
