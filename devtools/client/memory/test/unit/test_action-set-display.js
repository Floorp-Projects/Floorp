/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the action creator `setCensusDisplay()` for display changing. Does not
 * test refreshing the census information, check `setCensusDisplayAndRefresh`
 * action for that.
 */

const { censusDisplays, censusState, viewState } = require("devtools/client/memory/constants");
const { setCensusDisplay } = require("devtools/client/memory/actions/census-display");
const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
const { changeView } = require("devtools/client/memory/actions/view");

// We test setting an invalid display, which triggers an assertion failure.
EXPECTED_DTU_ASSERT_FAILURE_COUNT = 1;

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  dispatch(changeView(viewState.CENSUS));

  // Test default display with no snapshots
  equal(getState().censusDisplay.breakdown.by, "coarseType",
        "default coarseType display selected at start.");

  dispatch(setCensusDisplay(censusDisplays.allocationStack));
  equal(getState().censusDisplay.breakdown.by, "allocationStack",
        "display changed with no snapshots");

  // Test invalid displays
  try {
    dispatch(setCensusDisplay({}));
    ok(false, "Throws when passing in an invalid display object");
  } catch (e) {
    ok(true, "Throws when passing in an invalid display object");
  }
  equal(getState().censusDisplay.breakdown.by, "allocationStack",
    "current display unchanged when passing invalid display");

  // Test new snapshots
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  await waitUntilCensusState(store, s => s.census, [censusState.SAVED]);
  equal(getState().snapshots[0].census.display, censusDisplays.allocationStack,
        "New snapshots use the current, non-default display");
});
