/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that we can show allocation stack displays in the tree.

"use strict";

const {
  takeSnapshotAndCensus,
} = require("resource://devtools/client/memory/actions/snapshot.js");
const censusDisplayActions = require("resource://devtools/client/memory/actions/census-display.js");
const { viewState } = require("resource://devtools/client/memory/constants.js");
const {
  changeView,
} = require("resource://devtools/client/memory/actions/view.js");

const TEST_URL =
  "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

this.test = makeMemoryTest(TEST_URL, async function ({ panel }) {
  const heapWorker = panel.panelWin.gHeapAnalysesClient;
  const { getState, dispatch } = panel.panelWin.gStore;
  const front = getState().front;
  const doc = panel.panelWin.document;

  dispatch(changeView(viewState.CENSUS));

  ok(!getState().allocations.recording, "Should not be recording allocagtions");

  await dispatch(takeSnapshotAndCensus(front, heapWorker));
  await dispatch(
    censusDisplayActions.setCensusDisplayAndRefresh(
      heapWorker,
      censusDisplays.allocationStack
    )
  );

  is(
    getState().censusDisplay.breakdown.by,
    "allocationStack",
    "Should be using allocation stack breakdown"
  );

  ok(
    !getState().allocations.recording,
    "Should still not be recording allocagtions"
  );

  ok(
    doc.querySelector(".no-allocation-stacks"),
    "Because we did not record allocations, " +
      "the no-allocation-stack warning should be visible"
  );
});
