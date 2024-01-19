/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function openContextMenu(itemElement, win = window) {
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    itemElement.ownerDocument,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(
    itemElement,
    {
      type: "contextmenu",
      button: 2,
    },
    win
  );
  let { target } = await popupShownPromise;
  return target;
}

async function testContextMenu() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let panelUIMenuButton = document.getElementById("PanelUI-menu-button");
    let contextMenu = await openContextMenu(panelUIMenuButton);
    let array1 = AppConstants.MENUBAR_CAN_AUTOHIDE
      ? [
          ".customize-context-moveToPanel",
          ".customize-context-removeFromToolbar",
          "#toolbarItemsMenuSeparator",
          "#toggle_toolbar-menubar",
          "#toggle_PersonalToolbar",
          "#viewToolbarsMenuSeparator",
          ".viewCustomizeToolbar",
        ]
      : [
          ".customize-context-moveToPanel",
          ".customize-context-removeFromToolbar",
          "#toolbarItemsMenuSeparator",
          "#toggle_PersonalToolbar",
          "#viewToolbarsMenuSeparator",
          ".viewCustomizeToolbar",
        ];
    let result1 = verifyContextMenu(contextMenu, array1);
    ok(!result1, "Expected no errors verifying context menu items");
    contextMenu.hidePopup();
    let onFullscreen = Promise.all([
      BrowserTestUtils.waitForEvent(window, "fullscreen"),
      BrowserTestUtils.waitForEvent(
        window,
        "sizemodechange",
        false,
        e => window.fullScreen
      ),
      BrowserTestUtils.waitForPopupEvent(contextMenu, "hidden"),
    ]);
    document.getElementById("View:FullScreen").doCommand();
    contextMenu.hidePopup();
    info("waiting for fullscreen");
    await onFullscreen;
    // make sure the toolbox is visible if it's autohidden
    document.getElementById("Browser:OpenLocation").doCommand();
    info("trigger the context menu");
    let contextMenu2 = await openContextMenu(panelUIMenuButton);
    info("context menu should be open, verify its menu items");
    let array2 = AppConstants.MENUBAR_CAN_AUTOHIDE
      ? [
          ".customize-context-moveToPanel",
          ".customize-context-removeFromToolbar",
          "#toolbarItemsMenuSeparator",
          "#toggle_toolbar-menubar",
          "#toggle_PersonalToolbar",
          "#viewToolbarsMenuSeparator",
          ".viewCustomizeToolbar",
          `menuseparator[contexttype="fullscreen"]`,
          `.fullscreen-context-autohide`,
          `menuitem[contexttype="fullscreen"]`,
        ]
      : [
          ".customize-context-moveToPanel",
          ".customize-context-removeFromToolbar",
          "#toolbarItemsMenuSeparator",
          "#toggle_PersonalToolbar",
          "#viewToolbarsMenuSeparator",
          ".viewCustomizeToolbar",
          `menuseparator[contexttype="fullscreen"]`,
          `.fullscreen-context-autohide`,
          `menuitem[contexttype="fullscreen"]`,
        ];
    let result2 = verifyContextMenu(contextMenu2, array2);
    ok(!result2, "Expected no errors verifying context menu items");
    let onExitFullscreen = Promise.all([
      BrowserTestUtils.waitForEvent(window, "fullscreen"),
      BrowserTestUtils.waitForEvent(
        window,
        "sizemodechange",
        false,
        e => !window.fullScreen
      ),
      BrowserTestUtils.waitForPopupEvent(contextMenu2, "hidden"),
    ]);
    document.getElementById("View:FullScreen").doCommand();
    contextMenu2.hidePopup();
    await onExitFullscreen;
  });
}

function verifyContextMenu(contextMenu, itemSelectors) {
  // Ignore hidden nodes
  let items = Array.from(contextMenu.children).filter(n =>
    BrowserTestUtils.isVisible(n)
  );
  let menuAsText = items
    .map(n => {
      return n.nodeName == "menuseparator"
        ? "---"
        : `${n.label} (${n.command})`;
    })
    .join("\n");
  info("Got actual context menu items: \n" + menuAsText);

  try {
    is(
      items.length,
      itemSelectors.length,
      "Context menu has the expected number of items"
    );
    for (let i = 0; i < items.length; i++) {
      let selector = itemSelectors[i];
      ok(
        items[i].matches(selector),
        `Item at ${i} matches expected selector: ${selector}`
      );
    }
  } catch (ex) {
    return ex;
  }
  return null;
}

add_task(testContextMenu);
