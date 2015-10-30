/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that we use the correct snapshot aggregate value
 * in `utils.getSnapshotTotals(snapshot)`
 */

let { breakdowns, snapshotState: states } = require("devtools/client/memory/constants");
let { getSnapshotTotals, breakdownEquals } = require("devtools/client/memory/utils");
let { toggleInvertedAndRefresh } = require("devtools/client/memory/actions/inverted");
let { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  let { getState, dispatch } = store;

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS]);

  ok(!getState().snapshots[0].inverted, "Snapshot is not inverted");
  ok(isBreakdownType(getState().snapshots[0].census, "coarseType"),
    "Snapshot using `coarseType` breakdown");

  let census = getState().snapshots[0].census;
  let result = aggregate(census);
  let totalBytes = result.bytes;
  let totalCount = result.count;

  ok(totalBytes > 0, "counted up bytes in the census");
  ok(totalCount > 0, "counted up count in the census");

  result = getSnapshotTotals(getState().snapshots[0])
  equal(totalBytes, result.bytes, "getSnapshotTotals reuslted in correct bytes");
  equal(totalCount, result.count, "getSnapshotTotals reuslted in correct count");

  dispatch(toggleInvertedAndRefresh(heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVING_CENSUS]);
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS]);
  ok(getState().snapshots[0].inverted, "Snapshot is inverted");

  result = getSnapshotTotals(getState().snapshots[0])
  equal(totalBytes, result.bytes, "getSnapshotTotals reuslted in correct bytes when inverted");
  equal(totalCount, result.count, "getSnapshotTotals reuslted in correct count when inverted");
});

function aggregate (census) {
  let totalBytes = census.bytes;
  let totalCount = census.count;
  for (let child of (census.children || [])) {
    let { bytes, count } = aggregate(child);
    totalBytes += bytes
    totalCount += count;
  }
  return { bytes: totalBytes, count: totalCount };
}
