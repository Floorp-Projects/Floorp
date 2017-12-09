/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html");

  // Install an extension.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["contextMenus"],
    },

    background: function() {
      browser.contextMenus.create({title: "a"});
      browser.contextMenus.create({title: "b"});
      browser.test.notifyPass("contextmenus-icons");
    },
  });

  await extension.startup();
  await extension.awaitFinish("contextmenus-icons");

  // Open the context menu.
  let contextMenu = await openContextMenu("#img1");

  // Confirm that the extension menu item exists.
  let topLevelExtensionMenuItems = contextMenu.getElementsByAttribute("ext-type", "top-level-menu");
  is(topLevelExtensionMenuItems.length, 1, "the top level extension menu item exists");

  await closeContextMenu();

  // Uninstall the extension.
  await extension.unload();

  // Open the context menu.
  contextMenu = await openContextMenu("#img1");

  // Confirm that the extension menu item has been removed.
  topLevelExtensionMenuItems = contextMenu.getElementsByAttribute("ext-type", "top-level-menu");
  is(topLevelExtensionMenuItems.length, 0, "no top level extension menu items should exist");

  await closeContextMenu();

  // Install a new extension.
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["contextMenus"],
    },
    background: function() {
      browser.contextMenus.create({title: "c"});
      browser.contextMenus.create({title: "d"});
      browser.test.notifyPass("contextmenus-uninstall-second-extension");
    },
  });

  await extension.startup();
  await extension.awaitFinish("contextmenus-uninstall-second-extension");

  // Open the context menu.
  contextMenu = await openContextMenu("#img1");

  // Confirm that only the new extension menu item is in the context menu.
  topLevelExtensionMenuItems = contextMenu.getElementsByAttribute("ext-type", "top-level-menu");
  is(topLevelExtensionMenuItems.length, 1, "only one top level extension menu item should exist");

  // Close the context menu.
  await closeContextMenu();

  // Uninstall the extension.
  await extension.unload();

  // Open the context menu.
  contextMenu = await openContextMenu("#img1");

  // Confirm that no extension menu items exist.
  topLevelExtensionMenuItems = contextMenu.getElementsByAttribute("ext-type", "top-level-menu");
  is(topLevelExtensionMenuItems.length, 0, "no top level extension menu items should exist");

  await closeContextMenu();

  await BrowserTestUtils.removeTab(tab1);
});
