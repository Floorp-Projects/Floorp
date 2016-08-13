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

add_task(function* () {
  let { inspector } = yield openInspectorForURL(TEST_PAGE);

  yield testManuallyDeleteSelectedNode();
  yield testAutomaticallyDeleteSelectedNode();
  yield testDeleteSelectedNodeContainerFrame();
  yield testDeleteWithNonElementNode();

  function* testManuallyDeleteSelectedNode() {
    info("Selecting a node, deleting it via context menu and checking that " +
          "its parent node is selected and breadcrumbs are updated.");

    yield deleteNodeWithContextMenu("#deleteManually");

    info("Performing checks.");
    yield assertNodeSelectedAndPanelsUpdated("#selectedAfterDelete",
                                             "li#selectedAfterDelete");
  }

  function* testAutomaticallyDeleteSelectedNode() {
    info("Selecting a node, deleting it via javascript and checking that " +
         "its parent node is selected and breadcrumbs are updated.");

    let div = yield getNodeFront("#deleteAutomatically", inspector);
    yield selectNode(div, inspector);

    info("Deleting selected node via javascript.");
    yield inspector.walker.removeNode(div);

    info("Waiting for inspector to update.");
    yield inspector.once("inspector-updated");

    info("Inspector updated, performing checks.");
    yield assertNodeSelectedAndPanelsUpdated("#deleteChildren",
                                             "ul#deleteChildren");
  }

  function* testDeleteSelectedNodeContainerFrame() {
    info("Selecting a node inside iframe, deleting the iframe via javascript " +
         "and checking the parent node of the iframe is selected and " +
         "breadcrumbs are updated.");

    info("Selecting an element inside iframe.");
    let iframe = yield getNodeFront("#deleteIframe", inspector);
    let div = yield getNodeFrontInFrame("#deleteInIframe", iframe, inspector);
    yield selectNode(div, inspector);

    info("Deleting selected node via javascript.");
    yield inspector.walker.removeNode(iframe);

    info("Waiting for inspector to update.");
    yield inspector.once("inspector-updated");

    info("Inspector updated, performing checks.");
    yield assertNodeSelectedAndPanelsUpdated("body", "body");
  }

  function* testDeleteWithNonElementNode() {
    info("Selecting a node, deleting it via context menu and checking that " +
         "its parent node is selected and breadcrumbs are updated " +
         "when the node is followed by a non-element node");

    yield deleteNodeWithContextMenu("#deleteWithNonElement");

    let expectedCrumbs = ["html", "body", "div#deleteToMakeSingleTextNode"];
    yield assertNodeSelectedAndCrumbsUpdated(expectedCrumbs,
                                             Node.TEXT_NODE);

    // Delete node with key, as cannot delete text node with
    // context menu at this time.
    inspector.markup._frame.focus();
    EventUtils.synthesizeKey("VK_DELETE", {});
    yield inspector.once("inspector-updated");

    expectedCrumbs = ["html", "body", "div#deleteToMakeSingleTextNode"];
    yield assertNodeSelectedAndCrumbsUpdated(expectedCrumbs,
                                             Node.ELEMENT_NODE);
  }

  function* deleteNodeWithContextMenu(selector) {
    yield selectNode(selector, inspector);
    let nodeToBeDeleted = inspector.selection.nodeFront;

    info("Getting the node container in the markup view.");
    let container = yield getContainerForSelector(selector, inspector);

    let allMenuItems = openContextMenuAndGetAllItems(inspector, {
      target: container.tagLine,
    });
    let menuItem = allMenuItems.find(item => item.id === "node-menu-delete");

    info("Clicking 'Delete Node' in the context menu.");
    is(menuItem.disabled, false, "delete menu item is enabled");
    menuItem.click();

    // close the open context menu
    EventUtils.synthesizeKey("VK_ESCAPE", {});

    info("Waiting for inspector to update.");
    yield inspector.once("inspector-updated");

    // Since the mutations are sent asynchronously from the server, the
    // inspector-updated event triggered by the deletion might happen before
    // the mutation is received and the element is removed from the
    // breadcrumbs. See bug 1284125.
    if (inspector.breadcrumbs.indexOf(nodeToBeDeleted) > -1) {
      info("Crumbs haven't seen deletion. Waiting for breadcrumbs-updated.");
      yield inspector.once("breadcrumbs-updated");
    }

    return menuItem;
  }

  function* assertNodeSelectedAndCrumbsUpdated(expectedCrumbs,
                                               expectedNodeType) {
    info("Performing checks");
    let actualNodeType = inspector.selection.nodeFront.nodeType;
    is(actualNodeType, expectedNodeType, "The node has the right type");

    let breadcrumbs = inspector.panelDoc.querySelectorAll(
      "#inspector-breadcrumbs .html-arrowscrollbox-inner > *");
    is(breadcrumbs.length, expectedCrumbs.length,
       "Have the correct number of breadcrumbs");
    for (let i = 0; i < breadcrumbs.length; i++) {
      is(breadcrumbs[i].textContent, expectedCrumbs[i],
         "Text content for button " + i + " is correct");
    }
  }

  function* assertNodeSelectedAndPanelsUpdated(selector, crumbLabel) {
    let nodeFront = yield getNodeFront(selector, inspector);
    is(inspector.selection.nodeFront, nodeFront, "The right node is selected");

    let breadcrumbs = inspector.panelDoc.querySelector(
      "#inspector-breadcrumbs .html-arrowscrollbox-inner");
    is(breadcrumbs.querySelector("button[checked=true]").textContent,
       crumbLabel,
       "The right breadcrumb is selected");
  }
});
