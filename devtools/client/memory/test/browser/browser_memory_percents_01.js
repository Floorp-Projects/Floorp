/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that we calculate percentages in the tree.

"use strict";

const {
  takeSnapshotAndCensus,
} = require("resource://devtools/client/memory/actions/snapshot.js");
const { viewState } = require("resource://devtools/client/memory/constants.js");
const {
  changeView,
} = require("resource://devtools/client/memory/actions/view.js");

const TEST_URL =
  "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

function checkCells(cells) {
  Assert.greater(cells.length, 1, "Should have found some");
  // Ignore the first header cell.
  for (const cell of cells.slice(1)) {
    const percent = cell.querySelector(".heap-tree-percent");
    ok(percent, "should have a percent cell");
    ok(
      percent.textContent.match(/^\d?\d%$/),
      "should be of the form nn% or n%"
    );
  }
}

this.test = makeMemoryTest(TEST_URL, async function ({ panel }) {
  const heapWorker = panel.panelWin.gHeapAnalysesClient;
  const { getState, dispatch } = panel.panelWin.gStore;
  const front = getState().front;
  const doc = panel.panelWin.document;

  dispatch(changeView(viewState.CENSUS));

  await dispatch(takeSnapshotAndCensus(front, heapWorker));
  is(
    getState().censusDisplay.breakdown.by,
    "coarseType",
    "Should be using coarse type breakdown"
  );

  const bytesCells = [...doc.querySelectorAll(".heap-tree-item-bytes")];
  checkCells(bytesCells);

  const totalBytesCells = [
    ...doc.querySelectorAll(".heap-tree-item-total-bytes"),
  ];
  checkCells(totalBytesCells);

  const countCells = [...doc.querySelectorAll(".heap-tree-item-count")];
  checkCells(countCells);

  const totalCountCells = [
    ...doc.querySelectorAll(".heap-tree-item-total-count"),
  ];
  checkCells(totalCountCells);
});
