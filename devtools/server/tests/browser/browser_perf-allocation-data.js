/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we have allocation data coming from the front.
 */

"use strict";

add_task(async function() {
  const target = await addTabTarget(MAIN_DOMAIN + "doc_allocations.html");
  const front = await target.getFront("performance");

  const rec = await front.startRecording({
    withMarkers: true,
    withAllocations: true,
    withTicks: true,
  });

  await waitUntil(() => rec.getAllocations().frames.length);
  await waitUntil(() => rec.getAllocations().timestamps.length);
  await waitUntil(() => rec.getAllocations().sizes.length);
  await waitUntil(() => rec.getAllocations().sites.length);

  await front.stopRecording(rec);

  const { timestamps, sizes } = rec.getAllocations();

  is(
    timestamps.length,
    sizes.length,
    "we have the same amount of timestamps and sizes"
  );
  ok(
    timestamps.every(time => time > 0 && typeof time === "number"),
    "all timestamps have numeric values"
  );
  ok(
    sizes.every(n => n > 0 && typeof n === "number"),
    "all sizes are positive numbers"
  );

  await target.destroy();
  gBrowser.removeCurrentTab();
});
