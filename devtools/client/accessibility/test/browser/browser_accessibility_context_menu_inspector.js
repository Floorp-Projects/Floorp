/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const a11yAutoInitEnabled = Services.prefs.getBoolPref(
  "devtools.accessibility.auto-init.enabled",
  false
);

const TEST_URI = `
  <h1 id="h1">header</h1>
  <p id="p">paragraph</p>
  <span id="span-1">text</span>
  <span id="span-2">
    IamaverylongtextwhichdoesntfitinInlineTextChildReallyIdontIamtoobig
  </span>`;

async function openContextMenuForNode({ toolbox }, selector) {
  info("Selecting Inspector tab and opening a context menu");
  const inspector = await toolbox.selectTool("inspector");

  if (!selector) {
    ok(inspector.selection.isBodyNode(), "Default selection is a body node.");
  } else if (typeof selector === "string") {
    await selectNode(selector, inspector, "test");
  } else {
    const updated = inspector.once("inspector-updated");
    inspector.selection.setNodeFront(selector, { reason: "test" });
    await updated;
  }

  const menuUpdated = a11yAutoInitEnabled
    ? Promise.resolve()
    : inspector.once("node-menu-updated");
  const allMenuItems = openContextMenuAndGetAllItems(inspector);
  await menuUpdated;
  return allMenuItems;
}

function checkShowA11YPropertiesNode(allMenuItems, disabled) {
  const showA11YPropertiesNode = allMenuItems.find(
    item => item.id === "node-menu-showaccessibilityproperties"
  );
  ok(
    showA11YPropertiesNode,
    "the popup menu now has a show accessibility properties item"
  );
  is(
    showA11YPropertiesNode.disabled,
    disabled,
    `Show accessibility properties item has correct disabled state: ${disabled}`
  );
  return showA11YPropertiesNode;
}

async function checkAccessibleObjectSelection(
  { toolbox, panel },
  menuItem,
  isText
) {
  const inspector = await toolbox.getPanel("inspector");
  info(
    "Triggering 'Show Accessibility Properties' and waiting for " +
      "accessibility panel to open"
  );
  const panelSelected = toolbox.once("accessibility-selected");
  const objectSelected = panel.once("new-accessible-front-selected");
  menuItem.click();
  await panelSelected;
  const selected = await objectSelected;

  const expectedNode = isText
    ? inspector.selection.nodeFront.inlineTextChild
    : inspector.selection.nodeFront;
  const expectedSelected = await panel.accessibilityProxy.accessibleWalkerFront.getAccessibleFor(
    expectedNode
  );
  is(selected, expectedSelected, "Accessible front selected correctly");

  const doc = panel.panelWin.document;
  const propertiesTree = doc.querySelector(".tree");
  is(doc.activeElement, propertiesTree, "Properties list must be focused.");
  ok(
    isVisible(doc.querySelector(".treeTable .treeRow.selected")),
    "Selected row is visible."
  );
}

addA11YPanelTask(
  "Test show accessibility properties context menu.",
  TEST_URI,
  async function testShowAccessibilityPropertiesContextMenu(env) {
    // Load the inspector to ensure it to use in this test.
    await env.toolbox.loadTool("inspector");

    let allMenuItems = await openContextMenuForNode(env);
    let showA11YPropertiesNode = checkShowA11YPropertiesNode(
      allMenuItems,
      !a11yAutoInitEnabled
    );

    allMenuItems = await openContextMenuForNode(env, "#h1");
    showA11YPropertiesNode = checkShowA11YPropertiesNode(allMenuItems, false);
    await checkAccessibleObjectSelection(env, showA11YPropertiesNode);

    allMenuItems = await openContextMenuForNode(env, "#span-1");
    showA11YPropertiesNode = checkShowA11YPropertiesNode(allMenuItems, false);
    await checkAccessibleObjectSelection(env, showA11YPropertiesNode, true);

    allMenuItems = await openContextMenuForNode(env, "#span-2");
    showA11YPropertiesNode = checkShowA11YPropertiesNode(
      allMenuItems,
      !a11yAutoInitEnabled
    );

    const inspector = env.toolbox.getPanel("inspector");
    const span2 = await getNodeFront("#span-2", inspector);
    await inspector.markup.expandNode(span2);
    const { nodes } = await inspector.walker.children(span2);
    allMenuItems = await openContextMenuForNode(env, nodes[0]);
    showA11YPropertiesNode = checkShowA11YPropertiesNode(allMenuItems, false);
    await checkAccessibleObjectSelection(env, showA11YPropertiesNode, false);
  }
);
