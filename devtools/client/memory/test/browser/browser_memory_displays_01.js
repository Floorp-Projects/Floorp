/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the heap tree renders rows based on the display
 */

const TEST_URL =
  "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";
const { viewState } = require("devtools/client/memory/constants");
const { changeView } = require("devtools/client/memory/actions/view");

this.test = makeMemoryTest(TEST_URL, async function({ tab, panel }) {
  const { gStore, document } = panel.panelWin;

  const { dispatch } = panel.panelWin.gStore;

  function $$(selector) {
    return [...document.querySelectorAll(selector)];
  }
  dispatch(changeView(viewState.CENSUS));

  await takeSnapshot(panel.panelWin);

  await waitUntilState(
    gStore,
    state =>
      state.snapshots[0].census &&
      state.snapshots[0].census.state === censusState.SAVED
  );

  info("Check coarse type heap view");

  ["Function", "js::PropMap", "Object", "strings"].forEach(findNameCell);

  await setCensusDisplay(panel.panelWin, censusDisplays.allocationStack);
  info("Check allocation stack heap view");
  [L10N.getStr("tree-item.nostack")].forEach(findNameCell);

  function findNameCell(name) {
    const el = $$(".tree .heap-tree-item-name").find(
      e => e.textContent === name
    );
    ok(el, `Found heap tree item cell for ${name}.`);
  }
});
