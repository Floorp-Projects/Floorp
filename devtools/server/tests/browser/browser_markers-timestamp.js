/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get a "TimeStamp" marker.
 */
"use strict";

const {
  pmmConsoleMethod,
  pmmLoadFrameScripts,
  pmmClearFrameScripts,
} = require("devtools/client/performance/test/helpers/profiler-mm-utils");
const MARKER_NAME = "TimeStamp";

add_task(async function() {
  const target = await addTabTarget(MAIN_DOMAIN + "doc_perf.html");

  const front = await target.getFront("performance");
  const rec = await front.startRecording({ withMarkers: true });

  pmmLoadFrameScripts(gBrowser);
  pmmConsoleMethod("timeStamp");
  pmmConsoleMethod("timeStamp", "myLabel");

  const markers = await waitForMarkerType(
    front,
    MARKER_NAME,
    m => m.length >= 2
  );

  await front.stopRecording(rec);

  ok(
    markers.every(({ stack }) => typeof stack === "number"),
    "All markers have stack references."
  );
  ok(
    markers.every(({ name }) => name === "TimeStamp"),
    "All markers found are TimeStamp markers"
  );
  ok(markers.length === 2, "found 2 TimeStamp markers");
  ok(
    markers.every(
      ({ start, end }) => typeof start === "number" && start === end
    ),
    "All markers have equal start and end times"
  );
  is(
    markers[0].causeName,
    void 0,
    "Unlabeled timestamps have an empty causeName"
  );
  is(
    markers[1].causeName,
    "myLabel",
    "Labeled timestamps have correct causeName"
  );

  pmmClearFrameScripts();

  await target.destroy();
  gBrowser.removeCurrentTab();
});
