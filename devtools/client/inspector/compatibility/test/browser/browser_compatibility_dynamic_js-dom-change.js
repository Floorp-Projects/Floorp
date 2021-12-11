/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { COMPATIBILITY_ISSUE_TYPE } = require("devtools/shared/constants");

const {
  COMPATIBILITY_APPEND_NODE_COMPLETE,
  COMPATIBILITY_CLEAR_DESTROYED_NODES,
} = require("devtools/client/inspector/compatibility/actions/index");

// Test the behavior rules are dynamically added

const ISSUE_CURSOR = {
  type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
  property: "cursor",
  url: "https://developer.mozilla.org/docs/Web/CSS/cursor",
  deprecated: false,
  experimental: false,
};

const ISSUE_HYPHENS = {
  type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY_ALIASES,
  aliases: ["hyphens"],
  property: "hyphens",
  url: "https://developer.mozilla.org/docs/Web/CSS/hyphens",
  deprecated: false,
  experimental: false,
};

const TEST_URI = `
  <style>
    .child {
      cursor: grab;
    }
  </style>
  <body></body>
`;

add_task(async function() {
  info("Testing dynamic DOM mutation using JavaScript");
  const tab = await addTab(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
  );

  const {
    allElementsPane,
    inspector,
    selectedElementPane,
  } = await openCompatibilityView();

  info("Check initial issues");
  await assertIssueList(selectedElementPane, []);
  await assertIssueList(allElementsPane, []);

  info("Append nodes dynamically using JavaScript");
  await testNodeMutation(
    ".child",
    COMPATIBILITY_APPEND_NODE_COMPLETE,
    tab,
    inspector,
    selectedElementPane,
    allElementsPane,
    [ISSUE_CURSOR],
    [ISSUE_HYPHENS, ISSUE_CURSOR],
    async function() {
      const doc = content.document;
      const parent = doc.querySelector("body");

      const newElementWithIssue = doc.createElement("div");
      newElementWithIssue.style.hyphens = "none";

      const parentOfIssueElement = doc.createElement("div");
      parentOfIssueElement.classList.add("parent");
      const child = doc.createElement("div");
      child.classList.add("child");
      parentOfIssueElement.appendChild(child);

      parent.appendChild(newElementWithIssue);
      parent.appendChild(parentOfIssueElement);
    }
  );

  info("Remove node whose child has compatibility issue");
  await testNodeMutation(
    "div",
    COMPATIBILITY_CLEAR_DESTROYED_NODES,
    tab,
    inspector,
    selectedElementPane,
    allElementsPane,
    [ISSUE_HYPHENS],
    [ISSUE_HYPHENS],
    async function() {
      const doc = content.document;
      const parent = doc.querySelector(".parent");
      parent.remove();
    }
  );

  info("Remove node which has compatibility issue");
  await testNodeMutation(
    "body",
    COMPATIBILITY_CLEAR_DESTROYED_NODES,
    tab,
    inspector,
    selectedElementPane,
    allElementsPane,
    [],
    [],
    async function() {
      const doc = content.document;
      const issueElement = doc.querySelector("div");
      issueElement.remove();
    }
  );

  await removeTab(tab);
});

async function testNodeMutation(
  selector,
  action,
  tab,
  inspector,
  selectedElementPane,
  allElementsPane,
  expectedSelectedElementIssues,
  expectedAllElementsIssues,
  contentTaskFunction
) {
  let onPanelUpdate = Promise.all([
    inspector.once("markupmutation"),
    waitForDispatch(inspector.store, action),
  ]);
  info("Add a new node with issue and another node whose child has the issue");
  await ContentTask.spawn(tab.linkedBrowser, {}, contentTaskFunction);
  info("Wait for changes");
  await onPanelUpdate;

  onPanelUpdate = waitForUpdateSelectedNodeAction(inspector.store);
  await selectNode(selector, inspector);
  await onPanelUpdate;

  info("Check element issues");
  await assertIssueList(selectedElementPane, expectedSelectedElementIssues);

  info("Check all issues");
  await assertIssueList(allElementsPane, expectedAllElementsIssues);
}
