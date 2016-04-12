/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that we can show allocation stack displays in the tree.

"use strict";

const { toggleRecordingAllocationStacks } = require("devtools/client/memory/actions/allocations");
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

  dispatch(censusDisplayActions.setCensusDisplay(censusDisplays.invertedAllocationStack));
  is(getState().censusDisplay.breakdown.by, "allocationStack");

  yield dispatch(toggleRecordingAllocationStacks(front));
  ok(getState().allocations.recording);

  // Let some allocations build up.
  yield waitForTime(500);

  yield dispatch(takeSnapshotAndCensus(front, heapWorker));

  const names = [...doc.querySelectorAll(".frame-link-function-display-name")];
  ok(names.length, "Should have rendered some allocation stack tree items");
  ok(names.some(e => !!e.textContent.trim()),
     "And at least some of them should have functionDisplayNames");
});
