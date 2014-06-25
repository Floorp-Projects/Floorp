/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that when nodes are being deleted in the page, the current selection
// and therefore the markup view, css rule view, computed view, font view,
// box model view, and breadcrumbs, reset accordingly to show the right node

const TEST_PAGE = "http://mochi.test:8888/browser/browser/devtools/inspector/test/browser_inspector_bug_848731_reset_selection_on_delete.html";

function test() {
  let inspector, toolbox;

  // Create a tab, load test HTML, wait for load and start the tests
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(function() {
      openInspector((aInspector, aToolbox) => {
        inspector = aInspector;
        toolbox = aToolbox;
        startTests();
      });
    }, content);
  }, true);
  content.location = TEST_PAGE;

  function startTests() {
    testManuallyDeleteSelectedNode();
  }

  function getContainerForRawNode(rawNode) {
    let front = inspector.markup.walker.frontForRawNode(rawNode);
    let container = inspector.markup.getContainer(front);
    return container;
  }

  // 1 - select a node, right click, hit delete, verify that its parent is selected
  // and that all other tools are fine
  function testManuallyDeleteSelectedNode() {
    info("Deleting a node via the devtools contextual menu");

    // Select our div
    let div = content.document.getElementById("deleteManually");
    inspector.selection.setNode(div);
    inspector.once("inspector-updated", () => {
      is(inspector.selection.node, div, "Test node is selected");

      // Get the node container in the markup view
      let container = getContainerForRawNode(div);

      // Simulate right-click
      EventUtils.synthesizeMouse(container.tagLine, 2, 2,
        {type: "contextmenu", button: 2}, inspector.panelWin);

      // And react to the popup shown event
      let contextMenu = inspector.panelDoc.getElementById("inspectorPopupSet");
      contextMenu.addEventListener("popupshown", function contextShown() {
        contextMenu.removeEventListener("popupshown", contextShown, false);

        // Click on the delete sub-menu item
        inspector.panelDoc.getElementById("node-menu-delete").click();

        // Once updated, make sure eveything is in place, and move on
        inspector.once("inspector-updated", () => {
          let parent = content.document.getElementById("deleteChildren");
          assertNodeSelectedAndPanelsUpdated(parent, "ul#deleteChildren");
          testAutomaticallyDeleteSelectedNode();
        });
      }, false);
    });
  }

  // 2 - select a node, delete it via javascript, verify the same things as 1
  function testAutomaticallyDeleteSelectedNode() {
    info("Deleting a node via javascript");

    // Select our second div
    let div = content.document.getElementById("deleteAutomatically");
    inspector.selection.setNode(div);
    inspector.once("inspector-updated", () => {
      is(inspector.selection.node, div, "Test node is selected");

      // Now remove that div via javascript
      let parent = content.document.getElementById("deleteChildren");
      parent.removeChild(div);

      // Once updated, make sure eveything is in place, and move on
      inspector.once("inspector-updated", () => {
        assertNodeSelectedAndPanelsUpdated(parent, "ul#deleteChildren");
        testDeleteSelectedNodeContainerFrame();
      });
    });
  }

  // 3 - select a node inside an iframe, delete the iframe via javascript, verify that the default node is selected
  // and that all other tools are fine
  function testDeleteSelectedNodeContainerFrame() {
    info("Deleting an iframe via javascript");

    // Select our third test element, inside the iframe
    let iframe = content.document.getElementById("deleteIframe");
    let div = iframe.contentDocument.getElementById("deleteInIframe");
    inspector.selection.setNode(div);
    inspector.once("inspector-updated", () => {
      is(inspector.selection.node, div, "Test node is selected");

      // Now remove that parent iframe via javascript
      let parent = content.document.body;
      parent.removeChild(iframe);

      // Once updated, make sure eveything is in place, and move on
      inspector.once("inspector-updated", () => {
        assertNodeSelectedAndPanelsUpdated(parent, "body");
        endTests();
      });
    });
  }

  function endTests() {
    executeSoon(() => {
      toolbox.destroy();
      toolbox = inspector = null;
      gBrowser.removeCurrentTab();
      finish();
    });
  }

  function assertNodeSelectedAndPanelsUpdated(node, crumbLabel) {
    // Right node selected?
    is(inspector.selection.nodeFront, getNodeFront(node),
      "The right node is selected");

    // breadcrumbs updated?
    let breadcrumbs = inspector.panelDoc.getElementById("inspector-breadcrumbs");
    is(breadcrumbs.querySelector("button[checked=true]").textContent, crumbLabel,
      "The right breadcrumb is selected");
  }
}
