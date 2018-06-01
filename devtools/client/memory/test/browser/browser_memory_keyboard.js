/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1246570 - Check that when pressing on LEFT arrow, the parent tree node
// gets focused.

"use strict";

const {
  viewState
} = require("devtools/client/memory/constants");
const {
  takeSnapshotAndCensus
} = require("devtools/client/memory/actions/snapshot");
const { changeView } = require("devtools/client/memory/actions/view");

const TEST_URL = "http://example.com/browser/devtools/client/memory/test/browser/doc_steady_allocation.html";

function waitUntilFocused(store, node) {
  return waitUntilState(store, state =>
      state.snapshots.length === 1 &&
      state.snapshots[0].census &&
      state.snapshots[0].census.state === censusState.SAVED &&
      state.snapshots[0].census.focused &&
      state.snapshots[0].census.focused === node
  );
}

function waitUntilExpanded(store, node) {
  return waitUntilState(store, state =>
    state.snapshots[0] &&
    state.snapshots[0].census &&
    state.snapshots[0].census.expanded.has(node.id));
}

this.test = makeMemoryTest(TEST_URL, async function({ tab, panel }) {
  const heapWorker = panel.panelWin.gHeapAnalysesClient;
  const front = panel.panelWin.gFront;
  const store = panel.panelWin.gStore;
  const { getState, dispatch } = store;
  const doc = panel.panelWin.document;

  dispatch(changeView(viewState.CENSUS));

  is(getState().censusDisplay.breakdown.by, "coarseType");

  await dispatch(takeSnapshotAndCensus(front, heapWorker));
  const census = getState().snapshots[0].census;
  const root1 = census.report.children[0];
  const root2 = census.report.children[0];
  const root3 = census.report.children[0];
  const root4 = census.report.children[0];
  const child1 = root1.children[0];

  info("Click on first node.");
  const firstNode = doc.querySelector(".tree .heap-tree-item-name");
  EventUtils.synthesizeMouseAtCenter(firstNode, {}, panel.panelWin);
  await waitUntilFocused(store, root1);
  ok(true, "First root is selected after click.");

  info("Press DOWN key, expect second root focused.");
  EventUtils.synthesizeKey("VK_DOWN", {}, panel.panelWin);
  await waitUntilFocused(store, root2);
  ok(true, "Second root is selected after pressing DOWN arrow.");

  info("Press DOWN key, expect third root focused.");
  EventUtils.synthesizeKey("VK_DOWN", {}, panel.panelWin);
  await waitUntilFocused(store, root3);
  ok(true, "Third root is selected after pressing DOWN arrow.");

  info("Press DOWN key, expect fourth root focused.");
  EventUtils.synthesizeKey("VK_DOWN", {}, panel.panelWin);
  await waitUntilFocused(store, root4);
  ok(true, "Fourth root is selected after pressing DOWN arrow.");

  info("Press UP key, expect third root focused.");
  EventUtils.synthesizeKey("VK_UP", {}, panel.panelWin);
  await waitUntilFocused(store, root3);
  ok(true, "Third root is selected after pressing UP arrow.");

  info("Press UP key, expect second root focused.");
  EventUtils.synthesizeKey("VK_UP", {}, panel.panelWin);
  await waitUntilFocused(store, root2);
  ok(true, "Second root is selected after pressing UP arrow.");

  info("Press UP key, expect first root focused.");
  EventUtils.synthesizeKey("VK_UP", {}, panel.panelWin);
  await waitUntilFocused(store, root1);
  ok(true, "First root is selected after pressing UP arrow.");

  info("Press RIGHT key");
  EventUtils.synthesizeKey("VK_RIGHT", {}, panel.panelWin);
  await waitUntilExpanded(store, root1);
  ok(true, "Root node is expanded.");

  info("Press RIGHT key, expect first child focused.");
  EventUtils.synthesizeKey("VK_RIGHT", {}, panel.panelWin);
  await waitUntilFocused(store, child1);
  ok(true, "First child is selected after pressing RIGHT arrow.");

  info("Press LEFT key, expect first root focused.");
  EventUtils.synthesizeKey("VK_LEFT", {}, panel.panelWin);
  await waitUntilFocused(store, root1);
  ok(true, "First root is selected after pressing LEFT arrow.");
});
