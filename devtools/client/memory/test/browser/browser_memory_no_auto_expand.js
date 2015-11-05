/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that we can show allocation stack breakdowns in the tree.

"use strict";

const { waitForTime } = require("devtools/shared/DevToolsUtils");
const { breakdowns } = require("devtools/client/memory/constants");
const { toggleRecordingAllocationStacks } = require("devtools/client/memory/actions/allocations");
const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
const breakdownActions = require("devtools/client/memory/actions/breakdown");
const { toggleInvertedAndRefresh } = require("devtools/client/memory/actions/inverted");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
  const heapWorker = panel.panelWin.gHeapAnalysesClient;
  const front = panel.panelWin.gFront;
  const { getState, dispatch } = panel.panelWin.gStore;
  const doc = panel.panelWin.document;

  is(getState().breakdown.by, "coarseType");
  yield dispatch(takeSnapshotAndCensus(front, heapWorker));

  is(getState().allocations.recording, false);
  const recordingCheckbox = doc.getElementById("record-allocation-stacks-checkbox");
  EventUtils.synthesizeMouseAtCenter(recordingCheckbox, {}, panel.panelWin);
  is(getState().allocations.recording, true);

  const nameElems = [...doc.querySelectorAll(".heap-tree-item-field.heap-tree-item-name")];
  is(nameElems.length, 4, "Should get 4 items, one for each coarse type");
  ok(nameElems.some(e => e.textContent.trim() === "objects"), "One for coarse type 'objects'");
  ok(nameElems.some(e => e.textContent.trim() === "scripts"), "One for coarse type 'scripts'");
  ok(nameElems.some(e => e.textContent.trim() === "strings"), "One for coarse type 'strings'");
  ok(nameElems.some(e => e.textContent.trim() === "other"), "One for coarse type 'other'");

  for (let e of nameElems) {
    is(e.style.marginLeft, "0px",
       "None of the elements should be an indented/expanded child");
  }
});
