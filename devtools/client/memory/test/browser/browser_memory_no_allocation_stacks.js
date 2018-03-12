/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that we can show allocation stack displays in the tree.

"use strict";

const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
const censusDisplayActions = require("devtools/client/memory/actions/census-display");
const { viewState } = require("devtools/client/memory/constants");
const { changeView } = require("devtools/client/memory/actions/view");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
  const heapWorker = panel.panelWin.gHeapAnalysesClient;
  const front = panel.panelWin.gFront;
  const { getState, dispatch } = panel.panelWin.gStore;
  const doc = panel.panelWin.document;

  dispatch(changeView(viewState.CENSUS));

  ok(!getState().allocations.recording,
     "Should not be recording allocagtions");

  yield dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield dispatch(censusDisplayActions.setCensusDisplayAndRefresh(
    heapWorker,
    censusDisplays.allocationStack));

  is(getState().censusDisplay.breakdown.by, "allocationStack",
     "Should be using allocation stack breakdown");

  ok(!getState().allocations.recording,
     "Should still not be recording allocagtions");

  ok(doc.querySelector(".no-allocation-stacks"),
    "Because we did not record allocations, " +
    "the no-allocation-stack warning should be visible");
});
