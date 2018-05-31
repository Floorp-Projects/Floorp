/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  getHistoryEntries,
} = require("devtools/client/webconsole/selectors/history");

// Tests for menuitem functionality that doesn't fit into any specific category
const TEST_URL = URL_ROOT + "doc_inspector_menu.html";
add_task(async function() {
  let { inspector, toolbox, testActor } = await openInspectorForURL(TEST_URL);
  await testShowDOMProperties();
  await testDuplicateNode();
  await testDeleteNode();
  await testDeleteTextNode();
  await testDeleteRootNode();
  await testScrollIntoView();
  async function testShowDOMProperties() {
    info("Testing 'Show DOM Properties' menu item.");
    let allMenuItems = openContextMenuAndGetAllItems(inspector);
    let showDOMPropertiesNode =
      allMenuItems.find(item => item.id === "node-menu-showdomproperties");
    ok(showDOMPropertiesNode, "the popup menu has a show dom properties item");

    let consoleOpened = toolbox.once("webconsole-ready");

    info("Triggering 'Show DOM Properties' and waiting for inspector open");
    showDOMPropertiesNode.click();
    await consoleOpened;

    let webconsoleUI = toolbox.getPanel("webconsole").hud.ui;
    let messagesAdded = webconsoleUI.once("new-messages");
    await messagesAdded;
    info("Checking if 'inspect($0)' was evaluated");

    let state = webconsoleUI.consoleOutput.getStore().getState();
    ok(getHistoryEntries(state)[0] === "inspect($0)");
    await toolbox.toggleSplitConsole();
  }
  async function testDuplicateNode() {
    info("Testing 'Duplicate Node' menu item for normal elements.");

    await selectNode(".duplicate", inspector);
    is((await testActor.getNumberOfElementMatches(".duplicate")), 1,
       "There should initially be 1 .duplicate node");

    let allMenuItems = openContextMenuAndGetAllItems(inspector);
    let menuItem =
      allMenuItems.find(item => item.id === "node-menu-duplicatenode");
    ok(menuItem, "'Duplicate node' menu item should exist");

    info("Triggering 'Duplicate Node' and waiting for inspector to update");
    let updated = inspector.once("markupmutation");
    menuItem.click();
    await updated;

    is((await testActor.getNumberOfElementMatches(".duplicate")), 2,
       "The duplicated node should be in the markup.");

    let container = await getContainerForSelector(".duplicate + .duplicate",
                                                   inspector);
    ok(container, "A MarkupContainer should be created for the new node");
  }

  async function testDeleteNode() {
    info("Testing 'Delete Node' menu item for normal elements.");
    await selectNode("#delete", inspector);
    let allMenuItems = openContextMenuAndGetAllItems(inspector);
    let deleteNode = allMenuItems.find(item => item.id === "node-menu-delete");
    ok(deleteNode, "the popup menu has a delete menu item");
    let updated = inspector.once("inspector-updated");

    info("Triggering 'Delete Node' and waiting for inspector to update");
    deleteNode.click();
    await updated;

    ok(!(await testActor.hasNode("#delete")), "Node deleted");
  }

  async function testDeleteTextNode() {
    info("Testing 'Delete Node' menu item for text elements.");
    let { walker } = inspector;
    let divBefore = await walker.querySelector(walker.rootNode, "#nestedHiddenElement");
    let { nodes } = await walker.children(divBefore);
    await selectNode(nodes[0], inspector, "test-highlight");

    let allMenuItems = openContextMenuAndGetAllItems(inspector);
    let deleteNode = allMenuItems.find(item => item.id === "node-menu-delete");
    ok(deleteNode, "the popup menu has a delete menu item");
    ok(!deleteNode.disabled, "the delete menu item is not disabled");
    let updated = inspector.once("inspector-updated");

    info("Triggering 'Delete Node' and waiting for inspector to update");
    deleteNode.click();
    await updated;

    let divAfter = await walker.querySelector(walker.rootNode, "#nestedHiddenElement");
    let nodesAfter = (await walker.children(divAfter)).nodes;
    ok(nodesAfter.length == 0, "the node still had children");
  }

  async function testDeleteRootNode() {
    info("Testing 'Delete Node' menu item does not delete root node.");
    await selectNode("html", inspector);

    let allMenuItems = openContextMenuAndGetAllItems(inspector);
    let deleteNode = allMenuItems.find(item => item.id === "node-menu-delete");
    deleteNode.click();

    await new Promise(resolve => {
      executeSoon(resolve);
    });

    ok((await testActor.eval("!!document.documentElement")),
       "Document element still alive.");
  }

  function testScrollIntoView() {
    // Follow up bug to add this test - https://bugzilla.mozilla.org/show_bug.cgi?id=1154107
    todo(false, "Verify that node is scrolled into the viewport.");
  }
});
