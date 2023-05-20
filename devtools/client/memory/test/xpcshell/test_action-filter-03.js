/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changing filter state in the middle of taking a snapshot results in
// the properly fitered census.

const {
  snapshotState: states,
  censusState,
  viewState,
} = require("resource://devtools/client/memory/constants.js");
const {
  setFilterString,
  setFilterStringAndRefresh,
} = require("resource://devtools/client/memory/actions/filter.js");
const {
  takeSnapshotAndCensus,
} = require("resource://devtools/client/memory/actions/snapshot.js");
const {
  changeView,
} = require("resource://devtools/client/memory/actions/view.js");

add_task(async function () {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  dispatch(changeView(viewState.CENSUS));

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  await waitUntilSnapshotState(store, [states.SAVING]);

  dispatch(setFilterString("str"));

  await waitUntilCensusState(store, snapshot => snapshot.census, [
    censusState.SAVED,
  ]);
  equal(getState().filter, "str", "should want filtered trees");
  equal(
    getState().snapshots[0].census.filter,
    "str",
    "snapshot-we-were-in-the-middle-of-saving's census should be filtered"
  );

  dispatch(setFilterStringAndRefresh("", heapWorker));
  await waitUntilCensusState(store, snapshot => snapshot.census, [
    censusState.SAVING,
  ]);
  ok(true, "changing filter string retriggers census");
  ok(!getState().filter, "no longer filtering");

  dispatch(setFilterString("obj"));
  await waitUntilCensusState(store, snapshot => snapshot.census, [
    censusState.SAVED,
  ]);
  equal(getState().filter, "obj", "filtering for obj now");
  equal(
    getState().snapshots[0].census.filter,
    "obj",
    "census-we-were-in-the-middle-of-recomputing should be filtered again"
  );

  heapWorker.destroy();
  await front.detach();
});
