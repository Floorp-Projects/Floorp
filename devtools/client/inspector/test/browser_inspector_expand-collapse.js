/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that context menu items exapnd all and collapse are shown properly.

const TEST_URL = "data:text/html;charset=utf-8," +
                 "<div id='parent-node'><div id='child-node'></div></div>";

add_task(async function() {
  // Test is often exceeding time-out threshold, similar to Bug 1137765
  requestLongerTimeout(2);

  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Selecting the parent node");

  const front = await getNodeFrontForSelector("#parent-node", inspector);

  await selectNode(front, inspector);

  info("Simulating context menu click on the selected node container.");
  let allMenuItems = openContextMenuAndGetAllItems(inspector, {
    target: getContainerForNodeFront(front, inspector).tagLine,
  });
  let nodeMenuCollapseElement =
    allMenuItems.find(item => item.id === "node-menu-collapse");
  let nodeMenuExpandElement =
    allMenuItems.find(item => item.id === "node-menu-expand");

  ok(nodeMenuCollapseElement.disabled, "Collapse option is disabled");
  ok(!nodeMenuExpandElement.disabled, "ExpandAll option is enabled");

  info("Testing whether expansion works properly");
  nodeMenuExpandElement.click();

  info("Waiting for expansion to occur");
  await waitForMultipleChildrenUpdates(inspector);
  const markUpContainer = getContainerForNodeFront(front, inspector);
  ok(markUpContainer.expanded, "node has been successfully expanded");

  // reselecting node after expansion
  await selectNode(front, inspector);

  info("Testing whether collapse works properly");
  info("Simulating context menu click on the selected node container.");
  allMenuItems = openContextMenuAndGetAllItems(inspector, {
    target: getContainerForNodeFront(front, inspector).tagLine,
  });
  nodeMenuCollapseElement =
    allMenuItems.find(item => item.id === "node-menu-collapse");
  nodeMenuExpandElement =
    allMenuItems.find(item => item.id === "node-menu-expand");

  ok(!nodeMenuCollapseElement.disabled, "Collapse option is enabled");
  ok(!nodeMenuExpandElement.disabled, "ExpandAll option is enabled");
  nodeMenuCollapseElement.click();

  info("Waiting for collapse to occur");
  await waitForMultipleChildrenUpdates(inspector);
  ok(!markUpContainer.expanded, "node has been successfully collapsed");
});
