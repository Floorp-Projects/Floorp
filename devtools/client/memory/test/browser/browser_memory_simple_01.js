/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests taking snapshots and default states.
 */

const TEST_URL =
  "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";
const { viewState } = require("devtools/client/memory/constants");
const { changeView } = require("devtools/client/memory/actions/view");

this.test = makeMemoryTest(TEST_URL, async function({ tab, panel }) {
  const { gStore, document } = panel.panelWin;
  const { getState, dispatch } = gStore;

  dispatch(changeView(viewState.CENSUS));

  let snapshotEls = document.querySelectorAll(
    "#memory-tool-container .list li"
  );
  is(getState().snapshots.length, 0, "Starts with no snapshots in store");
  is(snapshotEls.length, 0, "No snapshots rendered");

  await takeSnapshot(panel.panelWin);
  snapshotEls = document.querySelectorAll("#memory-tool-container .list li");
  is(getState().snapshots.length, 1, "One snapshot was created in store");
  is(snapshotEls.length, 1, "One snapshot was rendered");
  ok(
    snapshotEls[0].classList.contains("selected"),
    "Only snapshot has `selected` class"
  );

  await takeSnapshot(panel.panelWin);
  snapshotEls = document.querySelectorAll("#memory-tool-container .list li");
  is(getState().snapshots.length, 2, "Two snapshots created in store");
  is(snapshotEls.length, 2, "Two snapshots rendered");
  ok(
    !snapshotEls[0].classList.contains("selected"),
    "First snapshot no longer has `selected` class"
  );
  ok(
    snapshotEls[1].classList.contains("selected"),
    "Second snapshot has `selected` class"
  );

  await waitUntilCensusState(gStore, s => s.census, [
    censusState.SAVED,
    censusState.SAVED,
  ]);

  ok(
    document.querySelector(".heap-tree-item-name"),
    "Should have rendered some tree items"
  );
});
