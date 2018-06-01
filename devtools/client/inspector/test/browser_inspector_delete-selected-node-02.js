/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that when nodes are being deleted in the page, the current selection
// and therefore the markup view, css rule view, computed view, font view,
// box model view, and breadcrumbs, reset accordingly to show the right node

const TEST_PAGE = URL_ROOT +
  "doc_inspector_delete-selected-node-02.html";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_PAGE);

  await testManuallyDeleteSelectedNode();
  await testAutomaticallyDeleteSelectedNode();
  await testDeleteSelectedNodeContainerFrame();
  await testDeleteWithNonElementNode();

  async function testManuallyDeleteSelectedNode() {
    info("Selecting a node, deleting it via context menu and checking that " +
          "its parent node is selected and breadcrumbs are updated.");

    await deleteNodeWithContextMenu("#deleteManually");

    info("Performing checks.");
    await assertNodeSelectedAndPanelsUpdated("#selectedAfterDelete",
                                             "li#selectedAfterDelete");
  }

  async function testAutomaticallyDeleteSelectedNode() {
    info("Selecting a node, deleting it via javascript and checking that " +
         "its parent node is selected and breadcrumbs are updated.");

    const div = await getNodeFront("#deleteAutomatically", inspector);
    await selectNode(div, inspector);

    info("Deleting selected node via javascript.");
    await inspector.walker.removeNode(div);

    info("Waiting for inspector to update.");
    await inspector.once("inspector-updated");

    info("Inspector updated, performing checks.");
    await assertNodeSelectedAndPanelsUpdated("#deleteChildren",
                                             "ul#deleteChildren");
  }

  async function testDeleteSelectedNodeContainerFrame() {
    info("Selecting a node inside iframe, deleting the iframe via javascript " +
         "and checking the parent node of the iframe is selected and " +
         "breadcrumbs are updated.");

    info("Selecting an element inside iframe.");
    const iframe = await getNodeFront("#deleteIframe", inspector);
    const div = await getNodeFrontInFrame("#deleteInIframe", iframe, inspector);
    await selectNode(div, inspector);

    info("Deleting selected node via javascript.");
    await inspector.walker.removeNode(iframe);

    info("Waiting for inspector to update.");
    await inspector.once("inspector-updated");

    info("Inspector updated, performing checks.");
    await assertNodeSelectedAndPanelsUpdated("body", "body");
  }

  async function testDeleteWithNonElementNode() {
    info("Selecting a node, deleting it via context menu and checking that " +
         "its parent node is selected and breadcrumbs are updated " +
         "when the node is followed by a non-element node");

    await deleteNodeWithContextMenu("#deleteWithNonElement");

    let expectedCrumbs = ["html", "body", "div#deleteToMakeSingleTextNode"];
    await assertNodeSelectedAndCrumbsUpdated(expectedCrumbs,
                                             Node.TEXT_NODE);

    // Delete node with key, as cannot delete text node with
    // context menu at this time.
    inspector.markup._frame.focus();
    EventUtils.synthesizeKey("KEY_Delete");
    await inspector.once("inspector-updated");

    expectedCrumbs = ["html", "body", "div#deleteToMakeSingleTextNode"];
    await assertNodeSelectedAndCrumbsUpdated(expectedCrumbs,
                                             Node.ELEMENT_NODE);
  }

  async function deleteNodeWithContextMenu(selector) {
    await selectNode(selector, inspector);
    const nodeToBeDeleted = inspector.selection.nodeFront;

    info("Getting the node container in the markup view.");
    const container = await getContainerForSelector(selector, inspector);

    const allMenuItems = openContextMenuAndGetAllItems(inspector, {
      target: container.tagLine,
    });
    const menuItem = allMenuItems.find(item => item.id === "node-menu-delete");

    info("Clicking 'Delete Node' in the context menu.");
    is(menuItem.disabled, false, "delete menu item is enabled");
    menuItem.click();

    // close the open context menu
    EventUtils.synthesizeKey("KEY_Escape");

    info("Waiting for inspector to update.");
    await inspector.once("inspector-updated");

    // Since the mutations are sent asynchronously from the server, the
    // inspector-updated event triggered by the deletion might happen before
    // the mutation is received and the element is removed from the
    // breadcrumbs. See bug 1284125.
    if (inspector.breadcrumbs.indexOf(nodeToBeDeleted) > -1) {
      info("Crumbs haven't seen deletion. Waiting for breadcrumbs-updated.");
      await inspector.once("breadcrumbs-updated");
    }

    return menuItem;
  }

  function assertNodeSelectedAndCrumbsUpdated(expectedCrumbs,
                                               expectedNodeType) {
    info("Performing checks");
    const actualNodeType = inspector.selection.nodeFront.nodeType;
    is(actualNodeType, expectedNodeType, "The node has the right type");

    const breadcrumbs = inspector.panelDoc.querySelectorAll(
      "#inspector-breadcrumbs .html-arrowscrollbox-inner > *");
    is(breadcrumbs.length, expectedCrumbs.length,
       "Have the correct number of breadcrumbs");
    for (let i = 0; i < breadcrumbs.length; i++) {
      is(breadcrumbs[i].textContent, expectedCrumbs[i],
         "Text content for button " + i + " is correct");
    }
  }

  async function assertNodeSelectedAndPanelsUpdated(selector, crumbLabel) {
    const nodeFront = await getNodeFront(selector, inspector);
    is(inspector.selection.nodeFront, nodeFront, "The right node is selected");

    const breadcrumbs = inspector.panelDoc.querySelector(
      "#inspector-breadcrumbs .html-arrowscrollbox-inner");
    is(breadcrumbs.querySelector("button[checked=true]").textContent,
       crumbLabel,
       "The right breadcrumb is selected");
  }
});
