/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get a "Parse HTML" marker when a page sets innerHTML.
 */

const TEST_URL = EXAMPLE_URL + "doc_innerHTML.html"

function* spawnTest () {
  let { target, front } = yield initBackend(TEST_URL);
  let markers = [];

  front.on("timeline-data", handler);
  let model = yield front.startRecording({ withTicks: true });

  yield waitUntil(() => {
    return markers.length;
  }, 100);

  front.off("timeline-data", handler);
  yield front.stopRecording(model);

  info(`Got ${markers.length} markers.`);

  ok(markers.every(({name}) => name === "Parse HTML"), "All markers found are Parse HTML markers");

  // Destroy the front before removing tab to ensure no
  // lingering requests
  yield front.destroy();
  yield removeTab(target.tab);
  finish();

  function handler (_, name, _markers) {
    if (name !== "markers") {
      return;
    }

    _markers.forEach(marker => {
      info(marker.name);
      if (marker.name === "Parse HTML") {
        markers.push(marker);
      }
    });
  }
}
