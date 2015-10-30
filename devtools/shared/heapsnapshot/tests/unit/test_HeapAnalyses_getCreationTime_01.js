/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the HeapAnalyses{Client,Worker} can get a HeapSnapshot's
// creation time.

function waitForTenMilliseconds() {
  const start = Date.now();
  while (Date.now() - start < 10) ;
}

function run_test() {
  run_next_test();
}

const BREAKDOWN = {
  by: "internalType",
  then: { by: "count", count: true, bytes: true }
};

add_task(function* () {
  const client = new HeapAnalysesClient();
  const start = Date.now() * 1000;

  // Because Date.now() is less precise than the snapshot's time stamp, give it
  // a little bit of head room.
  waitForTenMilliseconds();
  const snapshotFilePath = saveNewHeapSnapshot();
  waitForTenMilliseconds();
  const end = Date.now() * 1000;

  yield client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  let time = yield client.getCreationTime("/not/a/real/path", {
    breakdown: BREAKDOWN
  });
  equal(time, null, "getCreationTime returns `null` when snapshot does not exist");

  time = yield client.getCreationTime(snapshotFilePath, {
    breakdown: BREAKDOWN
  });
  ok(time >= start, "creation time occurred after start");
  ok(time <= end, "creation time occurred before end");

  client.destroy();
});
