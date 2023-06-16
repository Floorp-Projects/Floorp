/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that toggling diffing unselects all snapshots.

const {
  censusState,
  viewState,
} = require("resource://devtools/client/memory/constants.js");
const {
  toggleDiffing,
} = require("resource://devtools/client/memory/actions/diffing.js");
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

  equal(getState().diffing, null, "not diffing by default");

  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  await waitUntilCensusState(store, s => s.census, [
    censusState.SAVED,
    censusState.SAVED,
    censusState.SAVED,
  ]);

  ok(
    getState().snapshots.some(s => s.selected),
    "One of the new snapshots is selected"
  );

  dispatch(toggleDiffing());
  ok(getState().diffing, "now diffing after toggling");

  for (const s of getState().snapshots) {
    ok(
      !s.selected,
      "No snapshot should be selected after entering diffing mode"
    );
  }

  heapWorker.destroy();
  await front.detach();
});
