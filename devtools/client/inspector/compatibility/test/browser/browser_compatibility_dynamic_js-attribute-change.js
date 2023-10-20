/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  COMPATIBILITY_ISSUE_TYPE,
} = require("resource://devtools/shared/constants.js");

const {
  COMPATIBILITY_UPDATE_NODE_COMPLETE,
} = require("resource://devtools/client/inspector/compatibility/actions/index.js");

// Test the behavior rules are dynamically added

const ISSUE_OUTLINE_RADIUS = {
  type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
  property: "-moz-user-input",
  url: "https://developer.mozilla.org/docs/Web/CSS/-moz-user-input",
  deprecated: true,
  experimental: false,
};

const ISSUE_SCROLLBAR_WIDTH = {
  type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
  property: "scrollbar-width",
  url: "https://developer.mozilla.org/docs/Web/CSS/scrollbar-width",
  deprecated: false,
  experimental: false,
};

const TEST_URI = `
  <style>
    .issue {
      -moz-user-input: none;
    }
  </style>
  <body>
    <div class="test"></div>
  </body>
`;

add_task(async function () {
  info("Testing dynamic style change using JavaScript");
  const tab = await addTab(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
  );

  const { allElementsPane, inspector, selectedElementPane } =
    await openCompatibilityView();

  info("Testing inline style change due to JavaScript execution");
  const onPanelUpdate = waitForUpdateSelectedNodeAction(inspector.store);
  info("Select the div to undergo mutation");
  await selectNode(".test", inspector);
  await onPanelUpdate;

  info("Check initial issues");
  await assertIssueList(selectedElementPane, []);
  await assertIssueList(allElementsPane, []);

  info("Adding inline style with compatibility issue");
  await testAttributeMutation(
    tab,
    inspector,
    selectedElementPane,
    allElementsPane,
    [ISSUE_SCROLLBAR_WIDTH],
    [ISSUE_SCROLLBAR_WIDTH],
    async function () {
      content.document.querySelector(".test").style["scrollbar-width"] = "none";
    }
  );

  info("Adding a class with declarations having compatibility issue");
  await testAttributeMutation(
    tab,
    inspector,
    selectedElementPane,
    allElementsPane,
    [ISSUE_SCROLLBAR_WIDTH, ISSUE_OUTLINE_RADIUS],
    [ISSUE_SCROLLBAR_WIDTH, ISSUE_OUTLINE_RADIUS],
    async function () {
      content.document.querySelector(".test").classList.add("issue");
    }
  );

  info("Removing a class with declarations having compatibility issue");
  await testAttributeMutation(
    tab,
    inspector,
    selectedElementPane,
    allElementsPane,
    [ISSUE_SCROLLBAR_WIDTH],
    [ISSUE_SCROLLBAR_WIDTH],
    async function () {
      content.document.querySelector(".test").classList.remove("issue");
    }
  );

  await removeTab(tab);
});

async function testAttributeMutation(
  tab,
  inspector,
  selectedElementPane,
  allElementsPane,
  expectedSelectedElementIssues,
  expectedAllElementsIssues,
  contentTaskFunction
) {
  const onPanelUpdate = Promise.all([
    inspector.once("markupmutation"),
    waitForDispatch(inspector.store, COMPATIBILITY_UPDATE_NODE_COMPLETE),
  ]);
  info("Run the task in webpage context");
  await ContentTask.spawn(tab.linkedBrowser, {}, contentTaskFunction);
  info("Wait for changes to reflect");
  await onPanelUpdate;

  info("Check issues listed in selected element pane");
  await assertIssueList(selectedElementPane, expectedSelectedElementIssues);
  info("Check issues listed in all issues pane");
  await assertIssueList(allElementsPane, expectedAllElementsIssues);
}
