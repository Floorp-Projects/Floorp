/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "<h1 id=\"h1\">header</h1><p id=\"p\">paragraph</p>";

add_task(async function testNoShowAccessibilityPropertiesContextMenu() {
  const tab = await addTab(buildURL(TEST_URI));
  const { linkedBrowser: browser } = tab;

  const contextMenu = document.getElementById("contentAreaContextMenu");
  const awaitPopupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  await BrowserTestUtils.synthesizeMouse("#h1", 0, 0, {
    type: "contextmenu",
    button: 2,
    centered: true,
  }, browser);
  await awaitPopupShown;

  const inspectA11YPropsItem = contextMenu.querySelector("#context-inspect-a11y");
  ok(inspectA11YPropsItem.hidden, "Accessibility tools are not enabled.");
  contextMenu.hidePopup();
  gBrowser.removeCurrentTab();
});

addA11YPanelTask("Test show accessibility properties context menu in browser.",
  TEST_URI,
  async function({ panel, toolbox, browser }) {
    const headerSelector = "#h1";

    const contextMenu = document.getElementById("contentAreaContextMenu");
    const awaitPopupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
    await BrowserTestUtils.synthesizeMouse(headerSelector, 0, 0, {
      type: "contextmenu",
      button: 2,
      centered: true,
    }, browser);
    await awaitPopupShown;

    const inspectA11YPropsItem = contextMenu.querySelector("#context-inspect-a11y");

    info("Triggering 'Inspect Accessibility Properties' and waiting for " +
         "accessibility panel to open");
    inspectA11YPropsItem.click();
    contextMenu.hidePopup();

    const selected = await panel.once("new-accessible-front-selected");
    const expectedSelectedNode = await getNodeFront(headerSelector,
                                                  toolbox.getPanel("inspector"));
    const expectedSelected = await panel.walker.getAccessibleFor(expectedSelectedNode);
    is(toolbox.getCurrentPanel(), panel, "Accessibility panel is currently selected");
    is(selected, expectedSelected, "Accessible front selected correctly");
  });
