/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the task creator `setBreakdownAndRefreshAndRefresh()` for breakdown changing.
 * We test this rather than `setBreakdownAndRefresh` directly, as we use the refresh action
 * in the app itself composed from `setBreakdownAndRefresh`
 */

let { breakdowns, snapshotState: states } = require("devtools/client/memory/constants");
let { breakdownEquals } = require("devtools/client/memory/utils");
let { setBreakdownAndRefresh } = require("devtools/client/memory/actions/breakdown");
let { takeSnapshotAndCensus, selectSnapshotAndRefresh } = require("devtools/client/memory/actions/snapshot");

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
  dispatch(setBreakdownAndRefresh(heapWorker, breakdowns.objectClass.breakdown));
  equal(getState().breakdown.by, "objectClass", "breakdown changed with no snapshots");

  // Test invalid breakdowns
  ok(getState().errors.length === 0, "No error actions in the queue.");
  dispatch(setBreakdownAndRefresh(heapWorker, {}));
  yield waitUntilState(store, () => getState().errors.length === 1);
  ok(true, "Emits an error action when passing in an invalid breakdown object");

  equal(getState().breakdown.by, "objectClass",
    "current breakdown unchanged when passing invalid breakdown");

  // Test new snapshots
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS]);
  ok(isBreakdownType(getState().snapshots[0].census, "objectClass"),
    "New snapshots use the current, non-default breakdown");


  // Updates when changing breakdown during `SAVING`
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS, states.SAVING]);
  dispatch(setBreakdownAndRefresh(heapWorker, breakdowns.coarseType.breakdown));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS, states.SAVED_CENSUS]);

  ok(isBreakdownType(getState().snapshots[1].census, "coarseType"),
    "Breakdown can be changed while saving snapshots, uses updated breakdown in census");


  // Updates when changing breakdown during `SAVING_CENSUS`
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS, states.SAVED_CENSUS, states.SAVING_CENSUS]);
  dispatch(setBreakdownAndRefresh(heapWorker, breakdowns.objectClass.breakdown));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS, states.SAVED_CENSUS, states.SAVED_CENSUS]);

  ok(breakdownEquals(getState().snapshots[2].breakdown, breakdowns.objectClass.breakdown),
    "Breakdown can be changed while saving census, stores updated breakdown in snapshot");
  ok(isBreakdownType(getState().snapshots[2].census, "objectClass"),
    "Breakdown can be changed while saving census, uses updated breakdown in census");

  // Updates census on currently selected snapshot when changing breakdown
  ok(getState().snapshots[2].selected, "Third snapshot currently selected");
  dispatch(setBreakdownAndRefresh(heapWorker, breakdowns.internalType.breakdown));
  yield waitUntilState(store, () => isBreakdownType(getState().snapshots[2].census, "internalType"));
  ok(isBreakdownType(getState().snapshots[2].census, "internalType"),
    "Snapshot census updated when changing breakdowns after already generating one census");

  // Does not update unselected censuses
  ok(!getState().snapshots[1].selected, "Second snapshot unselected currently");
  ok(breakdownEquals(getState().snapshots[1].breakdown, breakdowns.coarseType.breakdown),
    "Second snapshot using `coarseType` breakdown still and not yet updated to correct breakdown");
  ok(isBreakdownType(getState().snapshots[1].census, "coarseType"),
    "Second snapshot using `coarseType` still for census and not yet updated to correct breakdown");

  // Updates to current breakdown when switching to stale snapshot
  dispatch(selectSnapshotAndRefresh(heapWorker, getState().snapshots[1]));
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS, states.SAVING_CENSUS, states.SAVED_CENSUS]);
  yield waitUntilSnapshotState(store, [states.SAVED_CENSUS, states.SAVED_CENSUS, states.SAVED_CENSUS]);

  ok(getState().snapshots[1].selected, "Second snapshot selected currently");
  ok(breakdownEquals(getState().snapshots[1].breakdown, breakdowns.internalType.breakdown),
    "Second snapshot using `internalType` breakdown and updated to correct breakdown");
  ok(isBreakdownType(getState().snapshots[1].census, "internalType"),
    "Second snapshot using `internalType` for census and updated to correct breakdown");
});
