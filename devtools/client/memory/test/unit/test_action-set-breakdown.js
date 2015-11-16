/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the action creator `setBreakdown()` for breakdown changing.
 * Does not test refreshing the census information, check `setBreakdownAndRefresh` action
 * for that.
 */

let { breakdowns, snapshotState: states } = require("devtools/client/memory/constants");
let { setBreakdown } = require("devtools/client/memory/actions/breakdown");
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

  // Test default breakdown with no snapshots
  equal(getState().breakdown.by, "coarseType", "default coarseType breakdown selected at start.");
  dispatch(setBreakdown(breakdowns.objectClass.breakdown));
  equal(getState().breakdown.by, "objectClass", "breakdown changed with no snapshots");

  // Test invalid breakdowns
  try {
    dispatch(setBreakdown({}));
    ok(false, "Throws when passing in an invalid breakdown object");
  } catch (e) {
    ok(true, "Throws when passing in an invalid breakdown object");
  }
  equal(getState().breakdown.by, "objectClass",
    "current breakdown unchanged when passing invalid breakdown");

  // Test new snapshots
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS]);
  ok(isBreakdownType(getState().snapshots[0].census.report, "objectClass"),
    "New snapshots use the current, non-default breakdown");
});
