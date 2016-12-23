/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that we use the correct snapshot aggregate value
 * in `utils.getSnapshotTotals(snapshot)`
 */

const { censusDisplays, viewState, censusState } = require("devtools/client/memory/constants");
const { getSnapshotTotals } = require("devtools/client/memory/utils");
const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
const { setCensusDisplayAndRefresh } = require("devtools/client/memory/actions/census-display");
const { changeView } = require("devtools/client/memory/actions/view");

function run_test() {
  run_next_test();
}

add_task(function* () {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  let { getState, dispatch } = store;

  dispatch(changeView(viewState.CENSUS));

  yield dispatch(setCensusDisplayAndRefresh(heapWorker,
                                            censusDisplays.allocationStack));

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilCensusState(store, s => s.census, [censusState.SAVED]);

  ok(!getState().snapshots[0].census.display.inverted, "Snapshot is not inverted");

  let census = getState().snapshots[0].census;
  let result = aggregate(census.report);
  let totalBytes = result.bytes;
  let totalCount = result.count;

  ok(totalBytes > 0, "counted up bytes in the census");
  ok(totalCount > 0, "counted up count in the census");

  result = getSnapshotTotals(getState().snapshots[0].census);
  equal(totalBytes, result.bytes, "getSnapshotTotals reuslted in correct bytes");
  equal(totalCount, result.count, "getSnapshotTotals reuslted in correct count");

  dispatch(setCensusDisplayAndRefresh(heapWorker,
                                      censusDisplays.invertedAllocationStack));

  yield waitUntilCensusState(store, s => s.census, [censusState.SAVING]);
  yield waitUntilCensusState(store, s => s.census, [censusState.SAVED]);
  ok(getState().snapshots[0].census.display.inverted, "Snapshot is inverted");

  result = getSnapshotTotals(getState().snapshots[0].census);
  equal(totalBytes, result.bytes,
        "getSnapshotTotals reuslted in correct bytes when inverted");
  equal(totalCount, result.count,
        "getSnapshotTotals reuslted in correct count when inverted");
});

function aggregate(report) {
  let totalBytes = report.bytes;
  let totalCount = report.count;
  for (let child of (report.children || [])) {
    let { bytes, count } = aggregate(child);
    totalBytes += bytes;
    totalCount += count;
  }
  return { bytes: totalBytes, count: totalCount };
}
