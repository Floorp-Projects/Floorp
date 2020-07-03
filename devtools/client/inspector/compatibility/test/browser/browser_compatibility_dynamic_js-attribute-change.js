/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const MDNCompatibility = require("devtools/shared/compatibility/MDNCompatibility");

const {
  COMPATIBILITY_UPDATE_NODE_COMPLETE,
} = require("devtools/client/inspector/compatibility/actions/index");

// Test the behavior rules are dynamically added

const ISSUE_CURSOR = {
  type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY,
  property: "cursor",
  url: "https://developer.mozilla.org/docs/Web/CSS/cursor",
  deprecated: false,
  experimental: false,
};

const ISSUE_HYPHENS = {
  type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY_ALIASES,
  aliases: ["hyphens"],
  property: "hyphens",
  url: "https://developer.mozilla.org/docs/Web/CSS/hyphens",
  deprecated: false,
  experimental: false,
};

const TEST_URI = `
  <style>
    .issue {
      cursor: grab;
    }
  </style>
  <body>
    <div class="test"></div>
  </body>
`;

add_task(async function() {
  info("Testing dynamic style change using JavaScript");
  const tab = await addTab(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
  );

  const {
    allElementsPane,
    inspector,
    selectedElementPane,
  } = await openCompatibilityView();

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
    [ISSUE_HYPHENS],
    [ISSUE_HYPHENS],
    async function() {
      content.document.querySelector(".test").style.hyphens = "none";
    }
  );

  info("Adding a class with declarations having compatibility issue");
  await testAttributeMutation(
    tab,
    inspector,
    selectedElementPane,
    allElementsPane,
    [ISSUE_HYPHENS, ISSUE_CURSOR],
    [ISSUE_HYPHENS, ISSUE_CURSOR],
    async function() {
      content.document.querySelector(".test").classList.add("issue");
    }
  );

  info("Removing a class with declarations having compatibility issue");
  await testAttributeMutation(
    tab,
    inspector,
    selectedElementPane,
    allElementsPane,
    [ISSUE_HYPHENS],
    [ISSUE_HYPHENS],
    async function() {
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
