/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { COMPATIBILITY_ISSUE_TYPE } = require("devtools/shared/constants");

const {
  COMPATIBILITY_APPEND_NODE_COMPLETE,
  COMPATIBILITY_REMOVE_NODE_COMPLETE,
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
    div {
      cursor: grab;
    }
  </style>
  <body>
    <div></div>
    <div class="parent">
      <div style="hyphens: none"></div>
    </div>
  </body>
`;

add_task(async function() {
  info("Testing dynamic DOM mutation using JavaScript");
  const tab = await addTab(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
  );

  const { allElementsPane, inspector } = await openCompatibilityView();

  info("Check initial issues");
  await assertIssueList(allElementsPane, [ISSUE_CURSOR, ISSUE_HYPHENS]);

  info("Delete node whose child node has CSS compatibility issue");
  await testNodeRemoval(".parent", inspector, allElementsPane, [ISSUE_CURSOR]);

  info("Delete node that has CSS compatibility issue");
  await testNodeRemoval("div", inspector, allElementsPane, []);

  info("Add node that has CSS compatibility issue");
  await testNodeAddition("div", inspector, allElementsPane, [ISSUE_CURSOR]);

  await removeTab(tab);
});

/**
 * Simulate a click on the markup-container (a line in the markup-view)
 * that corresponds to the selector passed.
 * This overrides the definition in inspector/test/head.js which times
 * out when the container to be clicked is already the selected node.
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {Promise} Resolves when the node has been selected.
 */
var clickContainer = async function(selector, inspector) {
  info("Clicking on the markup-container for node " + selector);

  const nodeFront = await getNodeFront(selector, inspector);
  const container = getContainerForNodeFront(nodeFront, inspector);

  const updated = container.selected
    ? Promise.resolve()
    : inspector.once("inspector-updated");
  EventUtils.synthesizeMouseAtCenter(
    container.tagLine,
    { type: "mousedown" },
    inspector.markup.doc.defaultView
  );
  EventUtils.synthesizeMouseAtCenter(
    container.tagLine,
    { type: "mouseup" },
    inspector.markup.doc.defaultView
  );
  return updated;
};

async function deleteNode(inspector, selector) {
  info("Select node " + selector + " and make sure it is focused");
  await selectNode(selector, inspector);
  await clickContainer(selector, inspector);

  info("Delete the node");
  const mutated = inspector.once("markupmutation");
  const updated = inspector.once("inspector-updated");
  EventUtils.sendKey("delete", inspector.panelWin);
  await mutated;
  await updated;
}

async function testNodeAddition(
  selector,
  inspector,
  allElementsPane,
  expectedAllElementsIssues
) {
  let onPanelUpdate = Promise.all([
    inspector.once("markupmutation"),
    waitForDispatch(inspector.store, COMPATIBILITY_APPEND_NODE_COMPLETE),
  ]);
  info("Add a new node");
  await inspector.addNode();
  await onPanelUpdate;

  onPanelUpdate = waitForUpdateSelectedNodeAction(inspector.store);
  await selectNode(selector, inspector);
  await onPanelUpdate;

  info("Check issues list for the webpage");
  await assertIssueList(allElementsPane, expectedAllElementsIssues);
}

async function testNodeRemoval(
  selector,
  inspector,
  allElementsPane,
  expectedAllElementsIssues
) {
  const onPanelUpdate = Promise.all([
    inspector.once("markupmutation"),
    waitForDispatch(inspector.store, COMPATIBILITY_REMOVE_NODE_COMPLETE),
  ]);
  info(`Delete the node with selector ${selector}`);
  await deleteNode(inspector, selector);
  await onPanelUpdate;

  info("Check issues list for the webpage");
  await assertIssueList(allElementsPane, expectedAllElementsIssues);
}
