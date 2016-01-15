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

  function* testManuallyDeleteSelectedNode() {
    info("Selecting a node, deleting it via context menu and checking that " +
          "its parent node is selected and breadcrumbs are updated.");

    yield selectNode("#deleteManually", inspector);

    info("Getting the node container in the markup view.");
    let container = yield getContainerForSelector("#deleteManually", inspector);

    info("Simulating right-click on the markup view container.");
    EventUtils.synthesizeMouse(container.tagLine, 2, 2,
      {type: "contextmenu", button: 2}, inspector.panelWin);

    info("Waiting for the context menu to open.");
    yield once(inspector.panelDoc.getElementById("inspectorPopupSet"), "popupshown");

    info("Clicking 'Delete Node' in the context menu.");
    inspector.panelDoc.getElementById("node-menu-delete").click();

    info("Waiting for inspector to update.");
    yield inspector.once("inspector-updated");

    info("Inspector updated, performing checks.");
    yield assertNodeSelectedAndPanelsUpdated("#selectedAfterDelete", "li#selectedAfterDelete");
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
    yield assertNodeSelectedAndPanelsUpdated("#deleteChildren", "ul#deleteChildren");
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

  function* assertNodeSelectedAndPanelsUpdated(selector, crumbLabel) {
    let nodeFront = yield getNodeFront(selector, inspector);
    is(inspector.selection.nodeFront, nodeFront, "The right node is selected");

    let breadcrumbs = inspector.panelDoc.getElementById("inspector-breadcrumbs");
    is(breadcrumbs.querySelector("button[checked=true]").textContent, crumbLabel,
      "The right breadcrumb is selected");
  }
});
