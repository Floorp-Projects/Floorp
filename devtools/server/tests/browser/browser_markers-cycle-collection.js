/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get "nsCycleCollector::Collect" and
 * "nsCycleCollector::ForgetSkippable" markers when we force cycle collection.
 */
"use strict";

const { PerformanceFront } = require("devtools/shared/fronts/performance");

add_task(async function() {
  // This test runs very slowly on linux32 debug EC2 instances.
  requestLongerTimeout(2);

  await addTab(MAIN_DOMAIN + "doc_force_cc.html");

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const front = PerformanceFront(client, form);
  await front.connect();
  const rec = await front.startRecording({ withMarkers: true });

  const markers = await waitForMarkerType(front,
    ["nsCycleCollector::Collect", "nsCycleCollector::ForgetSkippable"]);
  await front.stopRecording(rec);

  ok(markers.some(m => m.name === "nsCycleCollector::Collect"),
    "got some nsCycleCollector::Collect markers");
  ok(markers.some(m => m.name === "nsCycleCollector::ForgetSkippable"),
    "got some nsCycleCollector::Collect markers");

  await client.close();
  gBrowser.removeCurrentTab();
});
