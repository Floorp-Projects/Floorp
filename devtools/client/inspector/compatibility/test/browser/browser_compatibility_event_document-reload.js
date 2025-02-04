/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the issues after reloading the browsing document.

const TEST_URI = `
  <style>
  body {
    color: blue;
    overflow-anchor: auto;
    user-modify: read-only;
  }
  div {
    scrollbar-color: auto;
  }
  </style>
  <body>
    <div>test</div>
  </body>
`;

const TEST_DATA_SELECTED = [
  {
    property: "overflow-anchor",
    url: "https://developer.mozilla.org/docs/Web/CSS/overflow-anchor",
  },
  {
    property: "user-modify",
    url: "https://developer.mozilla.org/docs/Web/CSS/user-modify",
  },
];

const TEST_DATA_ALL = [
  ...TEST_DATA_SELECTED,
  {
    property: "scrollbar-color",
    url: "https://developer.mozilla.org/docs/Web/CSS/scrollbar-color",
  },
];

const {
  COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE,
  COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_FAILURE,
} = require("resource://devtools/client/inspector/compatibility/actions/index.js");

add_task(async function () {
  const tab = await addTab(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
  );

  const { allElementsPane, inspector, selectedElementPane } =
    await openCompatibilityView();

  info("Check the issues on the selected element");
  await assertIssueList(selectedElementPane, TEST_DATA_SELECTED);
  info("Check the issues on all elements");
  await assertIssueList(allElementsPane, TEST_DATA_ALL);

  let isUpdateSelectedNodeFailure = false;
  let isUpdateTopLevelTargetFailure = false;
  waitForDispatch(
    inspector.store,
    COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE
  ).then(() => {
    isUpdateSelectedNodeFailure = true;
  });
  waitForDispatch(
    inspector.store,
    COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_FAILURE
  ).then(() => {
    isUpdateTopLevelTargetFailure = true;
  });

  info("Reload the browsing page");
  const onReloaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  const onUpdateSelectedNode = waitForUpdateSelectedNodeAction(inspector.store);
  const onUpdateTopLevelTarget = waitForUpdateTopLevelTargetAction(
    inspector.store
  );
  gBrowser.reloadTab(tab);
  await Promise.all([onReloaded, onUpdateSelectedNode, onUpdateTopLevelTarget]);

  info("Check whether the failure action will be fired or not");
  ok(
    !isUpdateSelectedNodeFailure && !isUpdateTopLevelTargetFailure,
    "No error occurred"
  );

  info("Check the issues on the selected element again");
  await assertIssueList(selectedElementPane, TEST_DATA_SELECTED);
  info("Check the issues on all elements again");
  await assertIssueList(allElementsPane, TEST_DATA_ALL);
});
