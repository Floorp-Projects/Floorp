/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get a "Styles" marker with the correct meta.
 */


function* spawnTest () {
  let { target, front } = yield initBackend(SIMPLE_URL);
  let markers = [];

  front.on("timeline-data", handler);
  let model = yield front.startRecording({ withTicks: true });

  yield waitUntil(() => {
    return markers.some(({restyleHint}) => restyleHint != void 0);
  });

  front.off("timeline-data", handler);
  yield front.stopRecording(model);

  info(`Got ${markers.length} markers.`);

  ok(markers.every(({name}) => name === "Styles"), "All markers found are Styles markers");
  ok(markers.length, "found some restyle markers");

  ok(markers.some(({restyleHint}) => restyleHint != void 0), "some markers have a restyleHint property");

  yield removeTab(target.tab);
  finish();

  function handler (_, name, m) {
    if (name === "markers") {
      markers = markers.concat(m.filter(marker => marker.name === "Styles"));
    }
  }
}
