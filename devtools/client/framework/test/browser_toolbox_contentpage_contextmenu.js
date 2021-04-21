/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = "data:text/html;charset=utf8,<div>test content context menu</div>";

/**
 * Check that the DevTools context menu opens without triggering the content
 * context menu. See Bug 1591140.
 */
add_task(async function() {
  const tab = await addTab(URL);

  info("Test context menu conflict with dom.event.contextmenu.enabled=true");
  await pushPref("dom.event.contextmenu.enabled", true);
  await checkConflictWithContentPageMenu(tab);

  info("Test context menu conflict with dom.event.contextmenu.enabled=false");
  await pushPref("dom.event.contextmenu.enabled", false);
  await checkConflictWithContentPageMenu(tab);
});

async function checkConflictWithContentPageMenu(tab) {
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "inspector",
  });

  info("Check that the content page context menu works as expected");
  const contextMenu = document.getElementById("contentAreaContextMenu");
  is(contextMenu.state, "closed", "Content contextmenu is closed");

  info("Show the content context menu");
  const awaitPopupShown = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "div",
    {
      type: "contextmenu",
      button: 2,
      centered: true,
    },
    gBrowser.selectedBrowser
  );
  await awaitPopupShown;
  is(contextMenu.state, "open", "Content contextmenu is open");

  info("Hide the content context menu");
  const awaitPopupHidden = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await awaitPopupHidden;
  is(contextMenu.state, "closed", "Content contextmenu is closed again");

  info("Check the DevTools menu opens without opening the content menu");
  const onContextMenuPopup = toolbox.once("menu-open");
  // Use inspector search box for the test, any other element should be ok as
  // well.
  const inspector = toolbox.getPanel("inspector");
  synthesizeContextMenuEvent(inspector.searchBox);
  await onContextMenuPopup;

  const textboxContextMenu = toolbox.getTextBoxContextMenu();
  is(contextMenu.state, "closed", "Content contextmenu is still closed");
  is(textboxContextMenu.state, "open", "Toolbox contextmenu is open");

  info("Check that the toolbox context menu is closed when pressing ESCAPE");
  const onContextMenuHidden = toolbox.once("menu-close");
  if (Services.prefs.getBoolPref("widget.macos.native-context-menus", false)) {
    info("Using hidePopup semantics because of macOS native context menus.");
    textboxContextMenu.hidePopup();
  } else {
    EventUtils.sendKey("ESCAPE", toolbox.win);
  }
  await onContextMenuHidden;
  is(textboxContextMenu.state, "closed", "Toolbox contextmenu is closed.");

  await toolbox.destroy();
}
