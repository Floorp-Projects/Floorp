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

    let div = yield getNodeFront("#deleteToMakeSingleTextNode", inspector);
    let {nodes} = yield inspector.walker.children(div);
    let secondTextNode = nodes[1];
    yield deleteNodeWithKey(secondTextNode);
    expectedCrumbs = ["html", "body", "div#deleteToMakeSingleTextNode"];
    yield assertNodeSelectedAndCrumbsUpdated(expectedCrumbs,
                                             Node.ELEMENT_NODE);
  }

  function* deleteNodeWithKey(selector) {
    yield selectNode(selector, inspector);

    info("Simulating delete keypress on the markup view container.");
    EventUtils.synthesizeKey("VK_DELETE", {}, inspector.panelWin);

    info("Waiting for inspector to update.");
    yield inspector.once("inspector-updated");
  }

  function* deleteNodeWithContextMenu(selector) {
    yield selectNode(selector, inspector);

    info("Getting the node container in the markup view.");
    let container = yield getContainerForSelector(selector, inspector);

    info("Simulating right-click on the markup view container.");
    EventUtils.synthesizeMouse(container.tagLine, 2, 2,
      {type: "contextmenu", button: 2}, inspector.panelWin);

    info("Waiting for the context menu to open.");
    let popupSet = inspector.panelDoc.getElementById("inspectorPopupSet");
    yield once(popupSet, "popupshown");

    info("Clicking 'Delete Node' in the context menu.");
    inspector.panelDoc.getElementById("node-menu-delete").click();

    info("Waiting for inspector to update.");
    yield inspector.once("inspector-updated");
  }

  function* assertNodeSelectedAndCrumbsUpdated(expectedCrumbs,
                                               expectedNodeType) {
    info("Performing checks");
    let actualNodeType = inspector.selection.nodeFront.nodeType;
    is(actualNodeType, expectedNodeType, "The node has the right type");

    let breadcrumbs = inspector.panelDoc
                               .getElementById("inspector-breadcrumbs")
                               .childNodes;
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

    let breadcrumbs = inspector.panelDoc
                               .getElementById("inspector-breadcrumbs");
    is(breadcrumbs.querySelector("button[checked=true]").textContent,
       crumbLabel,
       "The right breadcrumb is selected");
  }
});
