/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get "MinorGC" markers when we continue to steadily allocate
 * objects.
 */
"use strict";

const { PerformanceFront } = require("devtools/shared/fronts/performance");

add_task(async function() {
  // This test runs very slowly on linux32 debug EC2 instances.
  requestLongerTimeout(2);

  await addTab(MAIN_DOMAIN + "doc_allocations.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = await connectDebuggerClient(client);
  let front = PerformanceFront(client, form);
  await front.connect();
  let rec = await front.startRecording({ withMarkers: true });

  let markers = await waitForMarkerType(front, ["MinorGC"]);
  await front.stopRecording(rec);

  ok(markers.some(m => m.name === "MinorGC" && m.causeName),
     "got some MinorGC markers");

  await client.close();
  gBrowser.removeCurrentTab();
});
