/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test top-level target switching for memory panel.

const { treeMapState } = require("devtools/client/memory/constants");
const PARENT_PROCESS_URI = "about:robots";
const CONTENT_PROCESS_URI = "data:text/plain,yo";

add_task(async () => {
  await pushPref("devtools.target-switching.enabled", true);

  info("Open the memory panel with empty page");
  const tab = await addTab();
  const { panel } = await openMemoryPanel(tab);
  const { gToolbox: toolbox, gStore: store } = panel.panelWin;

  info("Open a page running on the content process");
  await BrowserTestUtils.loadURI(tab.linkedBrowser, CONTENT_PROCESS_URI);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await takeAndWaitSnapshot(panel.panelWin, store);

  info("Navigate to a page running on parent process");
  await navigateTo(PARENT_PROCESS_URI, toolbox, tab);
  await takeAndWaitSnapshot(panel.panelWin, store);

  info("Return to a page running on content process again");
  await navigateTo(CONTENT_PROCESS_URI, toolbox, tab);
  await takeAndWaitSnapshot(panel.panelWin, store);

  info("Check the snapshots");
  const snapshots = store.getState().snapshots;
  const contentProcessPageNodes = getNodeNames(snapshots[0]);
  const parentProcessPageNodes = getNodeNames(snapshots[1]);
  const secondContentProcessPageNodes = getNodeNames(snapshots[2]);

  isnot(
    contentProcessPageNodes.sort().join(","),
    parentProcessPageNodes.sort().join(","),
    "Nodes on parent process page differ from content process page"
  );
  is(
    contentProcessPageNodes.sort().join(","),
    secondContentProcessPageNodes.sort().join(","),
    "Nodes on content process page are not changed after switching targets twice"
  );
});

function getNodeNames(snapshot) {
  const domNodePart = snapshot.treeMap.report.children.find(
    child => child.name === "domNode"
  );
  return domNodePart.children.map(child => child.name);
}

async function takeAndWaitSnapshot(window, store) {
  await takeSnapshot(window);

  const index = store.getState().snapshots.length - 1;
  await waitUntilState(
    store,
    state =>
      state.snapshots[index].treeMap &&
      state.snapshots[index].treeMap.state === treeMapState.SAVED
  );

  return store.getState().snapshots[index];
}

async function navigateTo(uri, toolbox, tab) {
  const onSwitched = once(toolbox, "switched-target");
  const onLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await BrowserTestUtils.loadURI(tab.linkedBrowser, uri);
  await onLoaded;
  await onSwitched;
  ok(true, "switched-target event is fired");
}
