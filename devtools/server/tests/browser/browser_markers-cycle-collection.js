/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get "nsCycleCollector::Collect" and
 * "nsCycleCollector::ForgetSkippable" markers when we force cycle collection.
 */
"use strict";

add_task(async function() {
  // This test runs very slowly on linux32 debug EC2 instances.
  requestLongerTimeout(2);

  const target = await addTabTarget(MAIN_DOMAIN + "doc_force_cc.html");
  const front = await target.getFront("performance");
  const rec = await front.startRecording({ withMarkers: true });

  const markers = await waitForMarkerType(front, [
    "nsCycleCollector::Collect",
    "nsCycleCollector::ForgetSkippable",
  ]);
  await front.stopRecording(rec);

  ok(
    markers.some(m => m.name === "nsCycleCollector::Collect"),
    "got some nsCycleCollector::Collect markers"
  );
  ok(
    markers.some(m => m.name === "nsCycleCollector::ForgetSkippable"),
    "got some nsCycleCollector::Collect markers"
  );

  await target.destroy();
  gBrowser.removeCurrentTab();
});
