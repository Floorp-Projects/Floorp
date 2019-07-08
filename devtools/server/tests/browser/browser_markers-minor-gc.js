/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get "MinorGC" markers when we continue to steadily allocate
 * objects.
 */
"use strict";

add_task(async function() {
  // This test runs very slowly on linux32 debug EC2 instances.
  requestLongerTimeout(2);

  const target = await addTabTarget(MAIN_DOMAIN + "doc_allocations.html");
  const front = await target.getFront("performance");

  const rec = await front.startRecording({ withMarkers: true });

  const markers = await waitForMarkerType(front, ["MinorGC"]);
  await front.stopRecording(rec);

  ok(
    markers.some(m => m.name === "MinorGC" && m.causeName),
    "got some MinorGC markers"
  );

  await target.destroy();
  gBrowser.removeCurrentTab();
});
