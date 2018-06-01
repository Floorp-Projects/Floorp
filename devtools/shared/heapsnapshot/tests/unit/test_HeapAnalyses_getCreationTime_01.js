/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the HeapAnalyses{Client,Worker} can get a HeapSnapshot's
// creation time.

function waitForThirtyMilliseconds() {
  const start = Date.now();
  while (Date.now() - start < 30) {
    // do nothing
  }
}

const BREAKDOWN = {
  by: "internalType",
  then: { by: "count", count: true, bytes: true }
};

add_task(async function() {
  const client = new HeapAnalysesClient();
  const start = Date.now() * 1000;

  // Because Date.now() is less precise than the snapshot's time stamp, give it
  // a little bit of head room. Additionally, WinXP's timers have a granularity
  // of only +/-15 ms.
  waitForThirtyMilliseconds();
  const snapshotFilePath = saveNewHeapSnapshot();
  waitForThirtyMilliseconds();
  const end = Date.now() * 1000;

  await client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  let threw = false;
  try {
    await client.getCreationTime("/not/a/real/path", {
      breakdown: BREAKDOWN
    });
  } catch (_) {
    threw = true;
  }
  ok(threw, "getCreationTime should throw when snapshot does not exist");

  const time = await client.getCreationTime(snapshotFilePath, {
    breakdown: BREAKDOWN
  });

  dumpn("Start = " + start);
  dumpn("End   = " + end);
  dumpn("Time  = " + time);

  ok(time >= start, "creation time occurred after start");
  ok(time <= end, "creation time occurred before end");

  client.destroy();
});
