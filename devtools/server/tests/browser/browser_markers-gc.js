/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get "GarbageCollection" markers.
 */
"use strict";

const { PerformanceFront } = require("devtools/shared/fronts/performance");
const MARKER_NAME = "GarbageCollection";

add_task(async function() {
  await addTab(MAIN_DOMAIN + "doc_force_gc.html");

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const front = PerformanceFront(client, form);
  await front.connect();
  const rec = await front.startRecording({ withMarkers: true });

  let markers = await waitForMarkerType(front, MARKER_NAME);
  await front.stopRecording(rec);

  ok(markers.some(m => m.name === MARKER_NAME), `got some ${MARKER_NAME} markers`);
  ok(markers.every(({causeName}) => typeof causeName === "string"),
    "All markers have a causeName.");
  ok(markers.every(({cycle}) => typeof cycle === "number"),
    "All markers have a `cycle` ID.");

  markers = rec.getMarkers();

  // Bug 1197646
  let ordered = true;
  markers.reduce((previousStart, current, i) => {
    if (i === 0) {
      return current.start;
    }
    if (current.start < previousStart) {
      ok(false, `markers must be in order. ${current.name} marker has later\
        start time (${current.start}) thanprevious: ${previousStart}`);
      ordered = false;
    }
    return current.start;
  });

  is(ordered, true, "All GC and non-GC markers are in order by start time.");

  await client.close();
  gBrowser.removeCurrentTab();
});
