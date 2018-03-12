/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that we calculate percentages in the tree.

"use strict";

const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
const { viewState } = require("devtools/client/memory/constants");
const { changeView } = require("devtools/client/memory/actions/view");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

function checkCells(cells) {
  ok(cells.length > 1, "Should have found some");
  // Ignore the first header cell.
  for (let cell of cells.slice(1)) {
    const percent = cell.querySelector(".heap-tree-percent");
    ok(percent, "should have a percent cell");
    ok(percent.textContent.match(/^\d?\d%$/), "should be of the form nn% or n%");
  }
}

this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
  const heapWorker = panel.panelWin.gHeapAnalysesClient;
  const front = panel.panelWin.gFront;
  const { getState, dispatch } = panel.panelWin.gStore;
  const doc = panel.panelWin.document;

  dispatch(changeView(viewState.CENSUS));

  yield dispatch(takeSnapshotAndCensus(front, heapWorker));
  is(getState().censusDisplay.breakdown.by, "coarseType",
     "Should be using coarse type breakdown");

  const bytesCells = [...doc.querySelectorAll(".heap-tree-item-bytes")];
  checkCells(bytesCells);

  const totalBytesCells = [...doc.querySelectorAll(".heap-tree-item-total-bytes")];
  checkCells(totalBytesCells);

  const countCells = [...doc.querySelectorAll(".heap-tree-item-count")];
  checkCells(countCells);

  const totalCountCells = [...doc.querySelectorAll(".heap-tree-item-total-count")];
  checkCells(totalCountCells);
});
