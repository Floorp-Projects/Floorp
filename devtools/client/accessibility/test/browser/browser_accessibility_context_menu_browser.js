/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = '<h1 id="h1">header</h1><p id="p">paragraph</p>';

addA11YPanelTask(
  "Test show accessibility properties context menu in browser.",
  TEST_URI,
  async function ({ panel, toolbox, browser }) {
    // Load the inspector to ensure it to use in this test.
    await toolbox.loadTool("inspector");

    const headerSelector = "#h1";

    const contextMenu = document.getElementById("contentAreaContextMenu");
    const awaitPopupShown = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popupshown"
    );
    await BrowserTestUtils.synthesizeMouse(
      headerSelector,
      0,
      0,
      {
        type: "contextmenu",
        button: 2,
        centered: true,
      },
      browser
    );
    await awaitPopupShown;

    const inspectA11YPropsItem = contextMenu.querySelector(
      "#context-inspect-a11y"
    );

    info(
      "Triggering 'Inspect Accessibility Properties' and waiting for " +
        "accessibility panel to open"
    );
    const popupHidden = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popuphidden"
    );
    contextMenu.activateItem(inspectA11YPropsItem);
    await popupHidden;

    const selected = await panel.once("new-accessible-front-selected");
    const expectedSelectedNode = await getNodeFront(
      headerSelector,
      toolbox.getPanel("inspector")
    );
    const expectedSelected =
      await panel.accessibilityProxy.accessibilityFront.accessibleWalkerFront.getAccessibleFor(
        expectedSelectedNode
      );
    is(
      toolbox.getCurrentPanel(),
      panel,
      "Accessibility panel is currently selected"
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
);
