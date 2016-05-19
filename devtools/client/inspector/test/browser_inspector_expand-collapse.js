/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that context menu items exapnd all and collapse are shown properly.

const TEST_URL = "data:text/html;charset=utf-8,<div id='parent-node'><div id='child-node'></div></div>";

add_task(function* () {

    // Test is often exceeding time-out threshold, similar to Bug 1137765
  requestLongerTimeout(2);

  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);

  let nodeMenuCollapseElement = inspector.panelDoc.getElementById("node-menu-collapse");
  let nodeMenuExpandElement = inspector.panelDoc.getElementById("node-menu-expand");

  info("Selecting the parent node");

  let front = yield getNodeFrontForSelector("#parent-node", inspector);

  yield selectNode(front, inspector);

  info("Simulating context menu click on the selected node container.");
  contextMenuClick(getContainerForNodeFront(front, inspector).tagLine);

  ok(nodeMenuCollapseElement.hasAttribute("disabled"), "Collapse option is disabled");

  ok(!nodeMenuExpandElement.hasAttribute("disabled"), "ExpandAll option is enabled");

  info("Testing whether expansion works properly");
  dispatchCommandEvent(nodeMenuExpandElement);
  info("Waiting for expansion to occur");
  yield waitForMultipleChildrenUpdates(inspector);
  let markUpContainer = getContainerForNodeFront(front, inspector);
  ok(markUpContainer.expanded, "node has been successfully expanded");

    // reslecting node after expansion
  yield selectNode(front, inspector);

  info("Testing whether collapse works properly");
  info("Simulating context menu click on the selected node container.");
  contextMenuClick(getContainerForNodeFront(front, inspector).tagLine);

  ok(!nodeMenuCollapseElement.hasAttribute("disabled"), "Collapse option is enabled");

  dispatchCommandEvent(nodeMenuCollapseElement);
  info("Waiting for collapse to occur");
  yield waitForMultipleChildrenUpdates(inspector);
  ok(!markUpContainer.expanded, "node has been successfully collapsed");
});
