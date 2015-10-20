/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the task creator `setBreakdownAndRefreshAndRefresh()` for custom
 * breakdowns.
 */

let { snapshotState: states } = require("devtools/client/memory/constants");
let { breakdownEquals } = require("devtools/client/memory/utils");
let { setBreakdownAndRefresh } = require("devtools/client/memory/actions/breakdown");
let { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
let custom = { by: "internalType", then: { by: "count", bytes: true }};

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  let { getState, dispatch } = store;

  dispatch(setBreakdownAndRefresh(heapWorker, custom));
  ok(breakdownEquals(getState().breakdown, custom),
    "Custom breakdown stored in breakdown state.");

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS]);

  ok(breakdownEquals(getState().snapshots[0].breakdown, custom),
    "New snapshot stored custom breakdown when done taking census");
  ok(getState().snapshots[0].census.children.length, "Census has some children");
  // Ensure we don't have `count` in any results
  ok(getState().snapshots[0].census.children.every(c => !c.count), "Census used custom breakdown");
});
