/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that we can show allocation stack breakdowns in the tree.

"use strict";

const { breakdowns } = require("devtools/client/memory/constants");
const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
const breakdownActions = require("devtools/client/memory/actions/breakdown");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
  const heapWorker = panel.panelWin.gHeapAnalysesClient;
  const front = panel.panelWin.gFront;
  const { getState, dispatch } = panel.panelWin.gStore;
  const doc = panel.panelWin.document;

  ok(!getState().allocations.recording,
     "Should not be recording allocagtions");

  yield dispatch(takeSnapshotAndCensus(front, heapWorker));
  yield dispatch(breakdownActions.setBreakdownAndRefresh(heapWorker,
                                                         breakdowns.allocationStack.breakdown));

  is(getState().breakdown.by, "allocationStack",
     "Should be using allocation stack breakdown");

  ok(!getState().allocations.recording,
     "Should still not be recording allocagtions");

  ok(doc.querySelector(".no-allocation-stacks"),
     "Because we did not record allocations, the no-allocation-stack warning should be visible");
});
