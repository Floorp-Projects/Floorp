/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1221150 - Ensure that census trees do not accidentally auto expand
// when clicking on the allocation stacks checkbox.

"use strict";

const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
const { viewState } = require("devtools/client/memory/constants");
const { changeView } = require("devtools/client/memory/actions/view");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

this.test = makeMemoryTest(TEST_URL, async function({ tab, panel }) {
  const heapWorker = panel.panelWin.gHeapAnalysesClient;
  const front = panel.panelWin.gFront;
  const { getState, dispatch } = panel.panelWin.gStore;
  const doc = panel.panelWin.document;

  dispatch(changeView(viewState.CENSUS));

  await dispatch(takeSnapshotAndCensus(front, heapWorker));

  is(getState().allocations.recording, false);
  const recordingCheckbox = doc.getElementById("record-allocation-stacks-checkbox");
  EventUtils.synthesizeMouseAtCenter(recordingCheckbox, {}, panel.panelWin);
  is(getState().allocations.recording, true);

  const nameElems = [
    ...doc.querySelectorAll(".heap-tree-item-field.heap-tree-item-name")
  ];

  for (const el of nameElems) {
    dumpn(`Found ${el.textContent.trim()}`);
    is(el.style.marginInlineStart, "0px",
       "None of the elements should be an indented/expanded child");
  }
});
