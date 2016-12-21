/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the async reducer responding to the action `takeCensus(heapWorker, snapshot)`
 */

var { snapshotState: states, censusDisplays, censusState, viewState } = require("devtools/client/memory/constants");
var actions = require("devtools/client/memory/actions/snapshot");
var { changeView } = require("devtools/client/memory/actions/view");

function run_test() {
  run_next_test();
}

// This tests taking a census on a snapshot that is still being read, which
// triggers an assertion failure.
EXPECTED_DTU_ASSERT_FAILURE_COUNT = 1;

add_task(function* () {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();

  store.dispatch(changeView(viewState.CENSUS));

  store.dispatch(actions.takeSnapshot(front));
  yield waitUntilState(store, () => {
    let snapshots = store.getState().snapshots;
    return snapshots.length === 1 && snapshots[0].state === states.SAVED;
  });

  let snapshot = store.getState().snapshots[0];
  equal(snapshot.census, null, "No census data exists yet on the snapshot.");

  // Test error case of wrong state.
  store.dispatch(actions.takeCensus(heapWorker, snapshot.id));
  yield waitUntilState(store, () => store.getState().errors.length === 1);

  dumpn("Found error: " + store.getState().errors[0]);
  ok(/Assertion failure/.test(store.getState().errors[0]),
    "Error thrown when taking a census of a snapshot that has not been read.");

  store.dispatch(actions.readSnapshot(heapWorker, snapshot.id));
  yield waitUntilState(store, () => store.getState().snapshots[0].state === states.READ);

  store.dispatch(actions.takeCensus(heapWorker, snapshot.id));
  yield waitUntilCensusState(store, s => s.census, [censusState.SAVING]);
  yield waitUntilCensusState(store, s => s.census, [censusState.SAVED]);

  snapshot = store.getState().snapshots[0];
  ok(snapshot.census, "Snapshot has census after saved census");
  ok(snapshot.census.report.children.length, "Census is in tree node form");
  equal(snapshot.census.display, censusDisplays.coarseType,
        "Snapshot stored correct display used for the census");
});
