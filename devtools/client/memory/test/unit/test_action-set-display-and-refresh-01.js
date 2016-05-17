/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests the task creator `setCensusDisplayAndRefreshAndRefresh()` for display
 * changing. We test this rather than `setCensusDisplayAndRefresh` directly, as
 * we use the refresh action in the app itself composed from
 * `setCensusDisplayAndRefresh`.
 */

let { censusDisplays, snapshotState: states, censusState, viewState } = require("devtools/client/memory/constants");
let { setCensusDisplayAndRefresh } = require("devtools/client/memory/actions/census-display");
let { takeSnapshotAndCensus, selectSnapshotAndRefresh } = require("devtools/client/memory/actions/snapshot");
const { changeView } = require("devtools/client/memory/actions/view");

function run_test() {
  run_next_test();
}

// We test setting an invalid display, which triggers an assertion failure.
EXPECTED_DTU_ASSERT_FAILURE_COUNT = 1;

add_task(function* () {
  let front = new StubbedMemoryFront();
  let heapWorker = new HeapAnalysesClient();
  yield front.attach();
  let store = Store();
  let { getState, dispatch } = store;

  dispatch(changeView(viewState.CENSUS));

  // Test default display with no snapshots
  equal(getState().censusDisplay.breakdown.by, "coarseType",
        "default coarseType display selected at start.");
  dispatch(setCensusDisplayAndRefresh(heapWorker,
                                      censusDisplays.allocationStack));
  equal(getState().censusDisplay.breakdown.by, "allocationStack",
        "display changed with no snapshots");

  // Test invalid displays
  ok(getState().errors.length === 0, "No error actions in the queue.");
  dispatch(setCensusDisplayAndRefresh(heapWorker, {}));
  yield waitUntilState(store, () => getState().errors.length === 1);
  ok(true, "Emits an error action when passing in an invalid display object");

  equal(getState().censusDisplay.breakdown.by, "allocationStack",
    "current display unchanged when passing invalid display");

  // Test new snapshots
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED]);

  equal(getState().snapshots[0].census.display, censusDisplays.allocationStack,
        "New snapshot's census uses correct display");

  // Updates when changing display during `SAVING`
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED, censusState.SAVING]);
  dispatch(setCensusDisplayAndRefresh(heapWorker, censusDisplays.coarseType));
  yield waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED, censusState.SAVED]);
  equal(getState().snapshots[1].census.display, censusDisplays.coarseType,
        "Changing display while saving a snapshot results in a census using the new display");


  // Updates when changing display during `SAVING_CENSUS`
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED,
                              censusState.SAVED,
                              censusState.SAVING]);
  dispatch(setCensusDisplayAndRefresh(heapWorker, censusDisplays.allocationStack));
  yield waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED,
                              censusState.SAVED,
                              censusState.SAVED]);
  equal(getState().snapshots[2].census.display, censusDisplays.allocationStack,
        "Display can be changed while saving census, stores updated display in snapshot");

  // Updates census on currently selected snapshot when changing display
  ok(getState().snapshots[2].selected, "Third snapshot currently selected");
  dispatch(setCensusDisplayAndRefresh(heapWorker, censusDisplays.coarseType));
  yield waitUntilState(store, state => state.snapshots[2].census.state === censusState.SAVING);
  yield waitUntilState(store, state => state.snapshots[2].census.state === censusState.SAVED);
  equal(getState().snapshots[2].census.display, censusDisplays.coarseType,
        "Snapshot census updated when changing displays after already generating one census");

  dispatch(setCensusDisplayAndRefresh(heapWorker, censusDisplays.allocationStack));
  yield waitUntilState(store, state => state.snapshots[2].census.state === censusState.SAVED);
  equal(getState().snapshots[2].census.display, censusDisplays.allocationStack,
        "Snapshot census updated when changing displays after already generating one census");

  // Does not update unselected censuses.
  ok(!getState().snapshots[1].selected, "Second snapshot selected currently");
  equal(getState().snapshots[1].census.display, censusDisplays.coarseType,
        "Second snapshot using `coarseType` display still and not yet updated to correct display");

  // Updates to current display when switching to stale snapshot.
  dispatch(selectSnapshotAndRefresh(heapWorker, getState().snapshots[1].id));
  yield waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED,
                              censusState.SAVING,
                              censusState.SAVED]);
  yield waitUntilCensusState(store, snapshot => snapshot.census,
                             [censusState.SAVED,
                              censusState.SAVED,
                              censusState.SAVED]);

  ok(getState().snapshots[1].selected, "Second snapshot selected currently");
  equal(getState().snapshots[1].census.display, censusDisplays.allocationStack,
        "Second snapshot using `allocationStack` display and updated to correct display");

  heapWorker.destroy();
  yield front.detach();
});
