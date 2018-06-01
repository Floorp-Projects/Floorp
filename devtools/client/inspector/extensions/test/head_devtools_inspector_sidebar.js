/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported getExtensionSidebarActors, expectNoSuchActorIDs,
            waitForObjectInspector, testSetExpressionSidebarPanel, assertTreeView,
            assertObjectInspector, moveMouseOnObjectInspectorDOMNode,
            moveMouseOnPanelCenter, clickOpenInspectorIcon */

"use strict";

ChromeUtils.defineModuleGetter(this, "ContentTaskUtils",
                               "resource://testing-common/ContentTaskUtils.jsm");

// Retrieve the array of all the objectValueGrip actors from the
// inspector extension sidebars state
// (used in browser_ext_devtools_panels_elements_sidebar.js).
function getExtensionSidebarActors(inspector) {
  const state = inspector.store.getState();

  const actors = [];

  for (const sidebarId of Object.keys(state.extensionsSidebar)) {
    const sidebarState = state.extensionsSidebar[sidebarId];

    if (sidebarState.viewMode === "object-value-grip-view" &&
        sidebarState.objectValueGrip && sidebarState.objectValueGrip.actor) {
      actors.push(sidebarState.objectValueGrip.actor);
    }
  }

  return actors;
}

// Test that the specified objectValueGrip actors have been released
// on the remote debugging server
// (used in browser_ext_devtools_panels_elements_sidebar.js).
async function expectNoSuchActorIDs(client, actors) {
  info(`Test that all the objectValueGrip actors have been released`);
  for (const actor of actors) {
    await Assert.rejects(client.request({to: actor, type: "requestTypes"}),
                         `No such actor for ID: ${actor}`);
  }
}

function waitForObjectInspector(panelDoc, waitForNodeWithType = "object") {
  const selector = `.object-inspector .objectBox-${waitForNodeWithType}`;
  return ContentTaskUtils.waitForCondition(() => {
    return panelDoc.querySelectorAll(selector).length > 0;
  });
}

// Helper function used inside the sidebar.setObjectValueGrip test case.
async function testSetExpressionSidebarPanel(panel, expected) {
  const {
    nodesLength,
    propertiesNames,
    rootTitle,
  } = expected;

  await waitForObjectInspector(panel);

  const objectInspectors = [...panel.querySelectorAll(".tree")];
  is(objectInspectors.length, 1, "There is the expected number of object inspectors");
  const [objectInspector] = objectInspectors;

  // Wait the objectInspector to have been fully rendered.
  await ContentTaskUtils.waitForCondition(() => {
    return objectInspector.querySelectorAll(".node").length >= nodesLength;
  });

  const oiNodes = objectInspector.querySelectorAll(".node");

  is(oiNodes.length, nodesLength, "Got the expected number of nodes in the tree");
  const propertiesNodes = [...objectInspector.querySelectorAll(".object-label")]
        .map(el => el.textContent);
  is(JSON.stringify(propertiesNodes), JSON.stringify(propertiesNames),
     "Got the expected property names");

  if (rootTitle) {
    // Also check that the ObjectInspector is rendered inside
    // an Accordion component with the expected title.
    const accordion = panel.querySelector(".accordion");

    ok(accordion, "Got an Accordion component as expected");

    is(accordion.querySelector("._content").firstChild, objectInspector,
       "The ObjectInspector should be inside the Accordion content");

    is(accordion.querySelector("._header").textContent.trim(), rootTitle,
       "The Accordion has the expected label");
  } else {
    // Also check that there is no Accordion component rendered
    // inside the sidebar panel.
    ok(!panel.querySelector(".accordion"),
       "Got no Accordion component as expected");
  }
}

function assertTreeView(panelDoc, expectedContent) {
  const {
    expectedTreeTables,
    expectedStringCells,
    expectedNumberCells
  } = expectedContent;

  if (expectedTreeTables) {
    is(panelDoc.querySelectorAll("table.treeTable").length, expectedTreeTables,
       "The panel document contains the expected number of TreeView components");
  }

  if (expectedStringCells) {
    is(panelDoc.querySelectorAll("table.treeTable .stringCell").length,
       expectedStringCells,
       "The panel document contains the expected number of string cells.");
  }

  if (expectedNumberCells) {
    is(panelDoc.querySelectorAll("table.treeTable .numberCell").length,
       expectedNumberCells,
       "The panel document contains the expected number of number cells.");
  }
}

async function assertObjectInspector(panelDoc, expectedContent) {
  const {expectedDOMNodes, expectedOpenInspectors} = expectedContent;

  // Get and verify the DOMNode and the "open inspector"" icon
  // rendered inside the ObjectInspector.
  const nodes = panelDoc.querySelectorAll(".objectBox-node");
  const nodeOpenInspectors = panelDoc.querySelectorAll(
    ".objectBox-node .open-inspector"
  );

  is(nodes.length, expectedDOMNodes,
     "Found the expected number of ObjectInspector DOMNodes");
  is(nodeOpenInspectors.length, expectedOpenInspectors,
     "Found the expected nuber of open-inspector icons inside the ObjectInspector");
}

function moveMouseOnObjectInspectorDOMNode(panelDoc, nodeIndex = 0) {
  const nodes = panelDoc.querySelectorAll(".objectBox-node");
  const node = nodes[nodeIndex];

  ok(node, "Found the ObjectInspector DOMNode");

  EventUtils.synthesizeMouseAtCenter(node, {type: "mousemove"},
                                     node.ownerDocument.defaultView);
}

function moveMouseOnPanelCenter(panelDoc) {
  EventUtils.synthesizeMouseAtCenter(panelDoc, {type: "mousemove"}, panelDoc.window);
}

function clickOpenInspectorIcon(panelDoc, nodeIndex = 0) {
  const nodes = panelDoc.querySelectorAll(".objectBox-node .open-inspector");
  const node = nodes[nodeIndex];

  ok(node, "Found the ObjectInspector open-inspector icon");

  node.click();
}
