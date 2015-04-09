/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get a "Parse HTML" marker when a page sets innerHTML.
 */

const TEST_URL = EXAMPLE_URL + "doc_innerHTML.html"

function* getMarkers(front) {
  const { promise, resolve } = Promise.defer();
  const handler = (_, markers) => {
    resolve(markers);
  };
  front.on("markers", handler);

  yield front.startRecording({ withTicks: true });

  const markers = yield promise;
  front.off("markers", handler);
  yield front.stopRecording();

  return markers;
}

function* spawnTest () {
  let { target, front } = yield initBackend(TEST_URL);

  const markers = yield getMarkers(front);
  info("markers = " + JSON.stringify(markers, null, 2));
  ok(markers.some(m => m.name === "Parse HTML" && m.stack != undefined),
     "Should get some Parse HTML markers");

  yield removeTab(target.tab);
  finish();
}
