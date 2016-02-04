/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the HeapAnalyses{Client,Worker} can take censuses.

function run_test() {
  run_next_test();
}

add_task(function* () {
  const client = new HeapAnalysesClient();

  const snapshotFilePath = saveNewHeapSnapshot();
  yield client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  const { report } = yield client.takeCensus(snapshotFilePath);
  ok(report, "Should get a report");
  equal(typeof report, "object", "report should be an object");

  ok(report.objects);
  ok(report.scripts);
  ok(report.strings);
  ok(report.other);

  client.destroy();
});
