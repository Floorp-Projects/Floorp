/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the task creator `setCensusDisplayAndRefreshAndRefresh()` for custom
 * displays.
 */

let { censusState, viewState } = require("devtools/client/memory/constants");
let { setCensusDisplayAndRefresh } = require("devtools/client/memory/actions/census-display");
let { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
let { changeView } = require("devtools/client/memory/actions/view");

let CUSTOM = {
  displayName: "Custom",
  tooltip: "Custom tooltip",
  inverted: false,
  breakdown: {
    by: "internalType",
    then: { by: "count", bytes: true, count: false }
  }
};

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
  dispatch(setCensusDisplayAndRefresh(heapWorker, CUSTOM));
  equal(getState().censusDisplay, CUSTOM,
        "CUSTOM display stored in display state.");

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield waitUntilCensusState(store, s => s.census, [censusState.SAVED]);

  equal(getState().snapshots[0].census.display, CUSTOM,
  "New snapshot stored CUSTOM display when done taking census");
  ok(getState().snapshots[0].census.report.children.length, "Census has some children");
  // Ensure we don't have `count` in any results
  ok(getState().snapshots[0].census.report.children.every(c => !c.count),
     "Census used CUSTOM display without counts");
  // Ensure we do have `bytes` in the results
  ok(getState().snapshots[0].census.report.children
               .every(c => typeof c.bytes === "number"),
     "Census used CUSTOM display with bytes");
});
