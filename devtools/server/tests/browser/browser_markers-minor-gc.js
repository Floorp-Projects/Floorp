/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get "MinorGC" markers when we continue to steadily allocate
 * objects.
 */
"use strict";

const { PerformanceFront } = require("devtools/shared/fronts/performance");

add_task(function* () {
  // This test runs very slowly on linux32 debug EC2 instances.
  requestLongerTimeout(2);

  yield addTab(MAIN_DOMAIN + "doc_allocations.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = PerformanceFront(client, form);
  yield front.connect();
  let rec = yield front.startRecording({ withMarkers: true });

  let markers = yield waitForMarkerType(front, ["MinorGC"]);
  yield front.stopRecording(rec);

  ok(markers.some(m => m.name === "MinorGC" && m.causeName),
     "got some MinorGC markers");

  yield client.close();
  gBrowser.removeCurrentTab();
});
