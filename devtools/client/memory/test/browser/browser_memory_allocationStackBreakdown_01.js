/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that we can show allocation stack breakdowns in the tree.

"use strict";

const { waitForTime } = require("devtools/shared/DevToolsUtils");
const { breakdowns } = require("devtools/client/memory/constants");
const { toggleRecordingAllocationStacks } = require("devtools/client/memory/actions/allocations");
const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
const breakdownActions = require("devtools/client/memory/actions/breakdown");
const { toggleInverted } = require("devtools/client/memory/actions/inverted");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
  const heapWorker = panel.panelWin.gHeapAnalysesClient;
  const front = panel.panelWin.gFront;
  const { getState, dispatch } = panel.panelWin.gStore;

  dispatch(toggleInverted());
  ok(getState().inverted, true);

  dispatch(breakdownActions.setBreakdown(breakdowns.allocationStack.breakdown));
  is(getState().breakdown.by, "allocationStack");

  yield dispatch(toggleRecordingAllocationStacks(front));
  ok(getState().allocations.recording);

  // Let some allocations build up.
  yield waitForTime(500);

  yield dispatch(takeSnapshotAndCensus(front, heapWorker));

  const doc = panel.panelWin.document;
  ok(doc.querySelector(".frame-link-function-display-name"),
     "Should have rendered some allocation stack tree items");
});
