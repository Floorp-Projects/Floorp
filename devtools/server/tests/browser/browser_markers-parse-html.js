/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get "Parse HTML" markers.
 */
"use strict";

const { PerformanceFront } = require("devtools/shared/fronts/performance");
const MARKER_NAME = "Parse HTML";

add_task(async function() {
  await addTab(MAIN_DOMAIN + "doc_innerHTML.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = await connectDebuggerClient(client);
  let front = PerformanceFront(client, form);
  await front.connect();
  let rec = await front.startRecording({ withMarkers: true });

  let markers = await waitForMarkerType(front, MARKER_NAME);
  await front.stopRecording(rec);

  ok(markers.some(m => m.name === MARKER_NAME), `got some ${MARKER_NAME} markers`);

  await client.close();
  gBrowser.removeCurrentTab();
});
