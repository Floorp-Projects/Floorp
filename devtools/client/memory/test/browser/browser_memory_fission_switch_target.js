/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test top-level target switching for memory panel.

const { treeMapState } = require("devtools/client/memory/constants");
const PARENT_PROCESS_URI = "about:robots";
const CONTENT_PROCESS_URI =
  "data:text/html,<section>content process page</section>";
const EXPECTED_ELEMENT_IN_PARENT_PROCESS = "button";
const EXPECTED_ELEMENT_IN_CONTENT_PROCESS = "section";

add_task(async () => {
  info("Open the memory panel with empty page");
  const tab = await addTab();
  const { panel } = await openMemoryPanel(tab);
  const { gStore: store } = panel.panelWin;

  info("Open a page running on the content process");
  await navigateTo(CONTENT_PROCESS_URI);
  await takeAndWaitSnapshot(
    panel.panelWin,
    store,
    EXPECTED_ELEMENT_IN_CONTENT_PROCESS
  );
  ok(true, "Can take a snapshot for content process page correctly");

  info("Navigate to a page running on parent process");
  await navigateTo(PARENT_PROCESS_URI);
  await takeAndWaitSnapshot(
    panel.panelWin,
    store,
    EXPECTED_ELEMENT_IN_PARENT_PROCESS
  );
  ok(true, "Can take a snapshot for parent process page correctly");

  info("Return to a page running on content process again");
  await navigateTo(CONTENT_PROCESS_URI);
  await takeAndWaitSnapshot(
    panel.panelWin,
    store,
    EXPECTED_ELEMENT_IN_CONTENT_PROCESS
  );
  ok(
    true,
    "Can take a snapshot for content process page correctly after switching targets twice"
  );
});

async function takeAndWaitSnapshot(window, store, expectedElement) {
  await asyncWaitUntil(async () => {
    await takeSnapshot(window);

    await waitUntilState(
      store,
      state =>
        state.snapshots[0].treeMap &&
        state.snapshots[0].treeMap.state === treeMapState.SAVED
    );

    const snapshot = store.getState().snapshots[0];
    const nodeNames = getNodeNames(snapshot);

    await clearSnapshots(window);

    return nodeNames.includes(expectedElement);
  });
}

function getNodeNames(snapshot) {
  const domNodePart = snapshot.treeMap.report.children.find(
    child => child.name === "domNode"
  );
  return domNodePart.children.map(child => child.name.toLowerCase());
}
