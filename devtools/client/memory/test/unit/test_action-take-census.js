/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the async reducer responding to the action `takeCensus(heapWorker, snapshot)`
 */

var { snapshotState: states, breakdowns } = require("devtools/client/memory/constants");
var { breakdownEquals } = require("devtools/client/memory/utils");
var { ERROR_TYPE } = require("devtools/client/shared/redux/middleware/task");
var actions = require("devtools/client/memory/actions/snapshot");

function run_test() {
  run_next_test();
}

add_task(function *() {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();

  store.dispatch(actions.takeSnapshot(front));
  yield waitUntilState(store, () => {
    let snapshots = store.getState().snapshots;
    return snapshots.length === 1 && snapshots[0].state === states.SAVED;
  });

  let snapshot = store.getState().snapshots[0];
  equal(snapshot.census, null, "No census data exists yet on the snapshot.");

  // Test error case of wrong state
  store.dispatch(actions.takeCensus(heapWorker, snapshot.id));
  yield waitUntilState(store, () => store.getState().errors.length === 1);
  ok(/Assertion failure/.test(store.getState().errors[0]),
    "Error thrown when taking a census of a snapshot that has not been read.");

  store.dispatch(actions.readSnapshot(heapWorker, snapshot.id));
  yield waitUntilState(store, () => store.getState().snapshots[0].state === states.READ);

  store.dispatch(actions.takeCensus(heapWorker, snapshot.id));
  yield waitUntilState(store, () => store.getState().snapshots[0].state === states.SAVING_CENSUS);
  yield waitUntilState(store, () => store.getState().snapshots[0].state === states.SAVED_CENSUS);

  snapshot = store.getState().snapshots[0];
  ok(snapshot.census, "Snapshot has census after saved census");
  ok(snapshot.census.report.children.length, "Census is in tree node form");
  ok(isBreakdownType(snapshot.census.report, "coarseType"),
    "Census is in tree node form with the default breakdown");
  ok(breakdownEquals(snapshot.census.breakdown, breakdowns.coarseType.breakdown),
    "Snapshot stored correct breakdown used for the census");
});
