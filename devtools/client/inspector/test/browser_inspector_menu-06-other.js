/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests for menuitem functionality that doesn't fit into any specific category
const TEST_URL = URL_ROOT + "doc_inspector_menu.html";
add_task(async function() {
  const { inspector, toolbox, testActor } = await openInspectorForURL(TEST_URL);
  await testShowDOMProperties();
  await testDuplicateNode();
  await testDeleteNode();
  await testDeleteTextNode();
  await testDeleteRootNode();
  await testScrollIntoView();

  async function testDuplicateNode() {
    info("Testing 'Duplicate Node' menu item for normal elements.");

    await selectNode(".duplicate", inspector);
    is(
      await testActor.getNumberOfElementMatches(".duplicate"),
      1,
      "There should initially be 1 .duplicate node"
    );

    const allMenuItems = openContextMenuAndGetAllItems(inspector);
    const menuItem = allMenuItems.find(
      item => item.id === "node-menu-duplicatenode"
    );
    ok(menuItem, "'Duplicate node' menu item should exist");

    info("Triggering 'Duplicate Node' and waiting for inspector to update");
    const updated = inspector.once("markupmutation");
    menuItem.click();
    await updated;

    is(
      await testActor.getNumberOfElementMatches(".duplicate"),
      2,
      "The duplicated node should be in the markup."
    );

    const container = await getContainerForSelector(
      ".duplicate + .duplicate",
      inspector
    );
    ok(container, "A MarkupContainer should be created for the new node");
  }

  async function testDeleteNode() {
    info("Testing 'Delete Node' menu item for normal elements.");
    await selectNode("#delete", inspector);
    const allMenuItems = openContextMenuAndGetAllItems(inspector);
    const deleteNode = allMenuItems.find(
      item => item.id === "node-menu-delete"
    );
    ok(deleteNode, "the popup menu has a delete menu item");
    const updated = inspector.once("inspector-updated");

    info("Triggering 'Delete Node' and waiting for inspector to update");
    deleteNode.click();
    await updated;

    ok(!(await testActor.hasNode("#delete")), "Node deleted");
  }

  async function testDeleteTextNode() {
    info("Testing 'Delete Node' menu item for text elements.");
    const { walker } = inspector;
    const divBefore = await walker.querySelector(
      walker.rootNode,
      "#nestedHiddenElement"
    );
    const { nodes } = await walker.children(divBefore);
    await selectNode(nodes[0], inspector, "test-highlight");

    const allMenuItems = openContextMenuAndGetAllItems(inspector);
    const deleteNode = allMenuItems.find(
      item => item.id === "node-menu-delete"
    );
    ok(deleteNode, "the popup menu has a delete menu item");
    ok(!deleteNode.disabled, "the delete menu item is not disabled");
    const updated = inspector.once("inspector-updated");

    info("Triggering 'Delete Node' and waiting for inspector to update");
    deleteNode.click();
    await updated;

    const divAfter = await walker.querySelector(
      walker.rootNode,
      "#nestedHiddenElement"
    );
    const nodesAfter = (await walker.children(divAfter)).nodes;
    ok(nodesAfter.length == 0, "the node still had children");
  }

  async function testDeleteRootNode() {
    info("Testing 'Delete Node' menu item does not delete root node.");
    await selectNode("html", inspector);

    const allMenuItems = openContextMenuAndGetAllItems(inspector);
    const deleteNode = allMenuItems.find(
      item => item.id === "node-menu-delete"
    );
    deleteNode.click();

    await new Promise(resolve => {
      executeSoon(resolve);
    });

    const hasDocumentElement = await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [],
      () => !!content.document.documentElement
    );
    ok(hasDocumentElement, "Document element still alive.");
  }

  async function testShowDOMProperties() {
    info("Testing 'Show DOM Properties' menu item.");
    const allMenuItems = openContextMenuAndGetAllItems(inspector);
    const showDOMPropertiesNode = allMenuItems.find(
      item => item.id === "node-menu-showdomproperties"
    );
    ok(showDOMPropertiesNode, "the popup menu has a show dom properties item");

    const consoleOpened = toolbox.once("webconsole-ready");

    info("Triggering 'Show DOM Properties' and waiting for inspector open");
    showDOMPropertiesNode.click();
    await consoleOpened;

    const webconsoleUI = toolbox.getPanel("webconsole").hud.ui;

    await poll(
      () => {
        const messages = [
          ...webconsoleUI.outputNode.querySelectorAll(".message"),
        ];
        const nodeMessage = messages.find(m => m.textContent.includes("body"));
        // wait for the object to be expanded
        return (
          nodeMessage &&
          nodeMessage.querySelectorAll(".object-inspector .node").length > 10
        );
      },
      "Waiting for the element node to be expanded",
      10,
      1000
    );

    info("Close split console");
    await toolbox.toggleSplitConsole();
  }

  function testScrollIntoView() {
    // Follow up bug to add this test - https://bugzilla.mozilla.org/show_bug.cgi?id=1154107
    todo(false, "Verify that node is scrolled into the viewport.");
  }
});
