/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get "Parse HTML" markers.
 */
"use strict";

const MARKER_NAME = "Parse HTML";

add_task(async function() {
  const target = await addTabTarget(MAIN_DOMAIN + "doc_innerHTML.html");

  const front = await target.getFront("performance");
  const rec = await front.startRecording({ withMarkers: true });

  const markers = await waitForMarkerType(front, MARKER_NAME);
  await front.stopRecording(rec);

  ok(
    markers.some(m => m.name === MARKER_NAME),
    `got some ${MARKER_NAME} markers`
  );

  await target.destroy();
  gBrowser.removeCurrentTab();
});
