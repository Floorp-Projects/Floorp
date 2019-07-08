/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// browser_ext_menus_events.js provides some coverage for viewTypes in normal
// tabs and extension popups.
// This test provides coverage for extension tabs and sidebars, as well as
// using the viewTypes property in menus.create and menus.update.

add_task(async function extension_tab_viewType() {
  async function background() {
    browser.menus.onShown.addListener(info => {
      browser.test.assertEq(
        "tabonly",
        info.menuIds.join(","),
        "Expected menu items"
      );
      browser.test.sendMessage("shown");
    });
    browser.menus.onClicked.addListener(info => {
      browser.test.assertEq("tab", info.viewType, "Expected viewType");
      browser.test.sendMessage("clicked");
    });

    browser.menus.create({
      id: "sidebaronly",
      title: "sidebar-only",
      viewTypes: ["sidebar"],
    });
    browser.menus.create(
      { id: "tabonly", title: "click here", viewTypes: ["tab"] },
      () => {
        browser.tabs.create({ url: "tab.html" });
      }
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    files: {
      "tab.html": `<!DOCTYPE html><meta charset="utf-8"><script src="tab.js"></script>`,
      "tab.js": `browser.test.sendMessage("ready");`,
    },
    background,
  });

  let extensionTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    null,
    true
  );
  await extension.startup();
  await extension.awaitMessage("ready");
  await extensionTabPromise;
  let menu = await openContextMenu();
  await extension.awaitMessage("shown");

  let menuItem = menu.getElementsByAttribute("label", "click here")[0];
  await closeExtensionContextMenu(menuItem);
  await extension.awaitMessage("clicked");

  // Unloading the extension will automatically close the extension's tab.html
  await extension.unload();
});

add_task(async function sidebar_panel_viewType() {
  async function sidebarJs() {
    browser.menus.onShown.addListener(info => {
      browser.test.assertEq(
        "sidebaronly",
        info.menuIds.join(","),
        "Expected menu items"
      );
      browser.test.assertEq("sidebar", info.viewType, "Expected viewType");
      browser.test.sendMessage("shown");
    });

    // Create menus and change their viewTypes using menus.update.
    browser.menus.create({
      id: "sidebaronly",
      title: "sidebaronly",
      viewTypes: ["tab"],
    });
    browser.menus.create({
      id: "tabonly",
      title: "tabonly",
      viewTypes: ["sidebar"],
    });
    await browser.menus.update("sidebaronly", { viewTypes: ["sidebar"] });
    await browser.menus.update("tabonly", { viewTypes: ["tab"] });
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary", // To automatically show sidebar on load.
    manifest: {
      permissions: ["menus"],
      sidebar_action: {
        default_panel: "sidebar.html",
      },
    },
    files: {
      "sidebar.html": `
        <!DOCTYPE html><meta charset="utf-8">
        <script src="sidebar.js"></script>
      `,
      "sidebar.js": sidebarJs,
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  let sidebarMenu = await openContextMenuInSidebar();
  await extension.awaitMessage("shown");
  await closeContextMenu(sidebarMenu);

  await extension.unload();
});
