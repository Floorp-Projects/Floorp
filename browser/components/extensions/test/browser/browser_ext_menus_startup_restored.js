"use strict";

/**
 * This tests verifies, that the startup cache for menus is not cleared on add-on
 * disable, but only on uninstall.
 */

function getMenuExtension(id, background) {
  return ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      manifest_version: 3,
      browser_specific_settings: { gecko: { id } },
      action: {
        default_area: "navbar",
      },
      permissions: ["menus"],
    },
    background,
  });
}

add_task(async function test_menu_create_id() {
  // Create a control extension to be able to communicate with the menu extension
  // after it has been disabled and re-enabled again.
  const controlExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: { gecko: { id: "control@mochi.test" } },
    },
    background: () => {
      browser.runtime.onMessageExternal.addListener(message =>
        browser.test.sendMessage(message)
      );
      browser.test.sendMessage("ready");
    },
  });
  await controlExtension.startup();
  await controlExtension.awaitMessage("ready");

  // Create a manifest V3 extension, which adds its menu entries only in an
  // onInstalled listener.
  const menuExtension = getMenuExtension("menu@mochi.test", () => {
    browser.runtime.onInstalled.addListener(() => {
      browser.menus.create(
        {
          id: "testItem",
          contexts: ["action"],
          title: "testItem",
        },
        () => {
          browser.runtime.sendMessage("control@mochi.test", "installed");
        }
      );
    });
    browser.runtime.sendMessage("control@mochi.test", "done");
  });
  await menuExtension.startup();
  await controlExtension.awaitMessage("done");
  await controlExtension.awaitMessage("installed");

  const buttonId = `${makeWidgetId(menuExtension.id)}-browser-action`;

  // Check if the action button and the context menu entry have been added.
  Assert.ok(
    !!window.document.getElementById(buttonId),
    "Button should have been created"
  );
  const menu = await openActionContextMenu(menuExtension, "browser");
  Assert.ok(
    [...menu.children].map(e => e.label).includes("testItem"),
    "Context menu should have been created"
  );
  await closeActionContextMenu();

  // Disable the extension and check that the action button has been removed.
  let menuAddon = await AddonManager.getAddonByID("menu@mochi.test");
  await menuAddon.disable();
  Assert.ok(
    !window.document.getElementById(buttonId),
    "Button should have been removed"
  );

  // Re-enable the extension and check, that the menu entry is correctly restored.
  await menuAddon.enable();
  await controlExtension.awaitMessage("done");
  Assert.ok(
    !!window.document.getElementById(buttonId),
    "Button should have been re-created"
  );
  const menu2 = await openActionContextMenu(menuExtension, "browser");
  Assert.ok(
    [...menu2.children].map(e => e.label).includes("testItem"),
    "Context menu should have been restored"
  );
  await closeActionContextMenu();

  // Uninstall the menu extension and check that the action button has been re-
  // moved.
  await menuExtension.unload();
  Assert.ok(
    !window.document.getElementById(buttonId),
    "Button should have been removed"
  );

  // Install the menu extension again, but do not add a menu entry this time.
  // Verify that the startup cache has been cleared on uninstall and the no longer
  // existing menu entry has not been restored.
  const noMenuExtension = getMenuExtension("menu@mochi.test", () => {
    browser.runtime.sendMessage("control@mochi.test", "done");
  });
  await noMenuExtension.startup();
  await controlExtension.awaitMessage("done");
  Assert.ok(
    !!window.document.getElementById(buttonId),
    "Button should have been re-created"
  );
  const menu3 = await openActionContextMenu(noMenuExtension, "browser");
  Assert.ok(
    ![...menu3.children].map(e => e.label).includes("testItem"),
    "Context menu should not have been restored"
  );
  await closeActionContextMenu();

  // Cleanup.
  await noMenuExtension.unload();
  await controlExtension.unload();
});
