/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = "<h1 id=\"h1\">header</h1><p id=\"p\">paragraph</p>";

addA11YPanelTask("Test show accessibility properties context menu.", TEST_URI,
  async function testShowAccessibilityPropertiesContextMenu({ panel, toolbox }) {
    let inspector = await toolbox.selectTool("inspector");

    ok(inspector.selection.isBodyNode(), "Default selection is a body node.");
    let menuUpdated = inspector.once("node-menu-updated");
    let allMenuItems = openContextMenuAndGetAllItems(inspector);
    let showA11YPropertiesNode = allMenuItems.find(item =>
      item.id === "node-menu-showaccessibilityproperties");
    ok(showA11YPropertiesNode,
      "the popup menu now has a show accessibility properties item");
    await menuUpdated;
    ok(showA11YPropertiesNode.disabled, "Body node does not have accessible");

    await selectNode("#h1", inspector, "test");
    menuUpdated = inspector.once("node-menu-updated");
    allMenuItems = openContextMenuAndGetAllItems(inspector);
    showA11YPropertiesNode = allMenuItems.find(item =>
      item.id === "node-menu-showaccessibilityproperties");
    ok(showA11YPropertiesNode,
      "the popup menu now has a show accessibility properties item");
    await menuUpdated;
    ok(!showA11YPropertiesNode.disabled, "Body node has an accessible");

    info("Triggering 'Show Accessibility Properties' and waiting for " +
         "accessibility panel to open");
    let panelSelected = toolbox.once("accessibility-selected");
    let objectSelected = panel.once("new-accessible-front-selected");
    showA11YPropertiesNode.click();
    await panelSelected;
    let selected = await objectSelected;

    let expectedSelected = await panel.walker.getAccessibleFor(
      inspector.selection.nodeFront);
    is(selected, expectedSelected, "Accessible front selected correctly");
  });
