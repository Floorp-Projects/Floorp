/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const PAGE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";

// Load an extension that has the "menus" permission. The returned Extension
// instance has a `callMenuApi` method to easily call a browser.menus method
// and wait for its result. It also emits the "onShown fired" message whenever
// the menus.onShown event is fired.
// The `getXULElementByMenuId` method returns the XUL element that corresponds
// to the menu item ID from the browser.menus API (if existent, null otherwise).
function loadExtensionWithMenusApi() {
  async function background() {
    browser.menus.onShown.addListener(() => {
      browser.test.sendMessage("onShown fired");
    });
    browser.test.onMessage.addListener((method, ...params) => {
      let result;
      if (method === "create") {
        result = new Promise(resolve => {
          browser.menus.create(params[0], resolve);
        });
      } else {
        result = browser.menus[method](...params);
      }
      result.then(() => {
        browser.test.sendMessage(`${method}-result`);
      });
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      browser_action: {},
      permissions: ["menus"],
    },
  });

  extension.callMenuApi = async function(method, ...params) {
    info(`Calling ${method}(${JSON.stringify(params)})`);
    extension.sendMessage(method, ...params);
    return extension.awaitMessage(`${method}-result`);
  };

  extension.getXULElementByMenuId = id => {
    // Same implementation as elementId getter in ext-menus.js
    if (typeof id != "number") {
      id = `_${id}`;
    }
    let xulId = `${makeWidgetId(extension.id)}-menuitem-${id}`;
    return document.getElementById(xulId);
  };

  return extension;
}

// Tests whether browser.menus.refresh works as expected with respect to the
// menu items that are added/updated/removed before/during/after opening a menu:
// - browser.refresh before a menu is shown should not have any effect.
// - browser.refresh while a menu is shown should update the menu.
// - browser.refresh after a menu is hidden should not have any effect.
async function testRefreshMenusWhileVisible({contexts, doOpenMenu, doCloseMenu,
                                            }) {
  let extension = loadExtensionWithMenusApi();
  await extension.startup();
  await extension.callMenuApi("create", {
    id: "abc",
    title: "first",
    contexts,
  });
  let elem = extension.getXULElementByMenuId("abc");
  is(elem, null, "Menu item should not be visible");

  // Refresh before a menu is shown - should be noop.
  await extension.callMenuApi("refresh");
  elem = extension.getXULElementByMenuId("abc");
  is(elem, null, "Menu item should still not be visible");

  // Open menu and expect menu to be rendered.
  await doOpenMenu(extension);
  elem = extension.getXULElementByMenuId("abc");
  is(elem.getAttribute("label"), "first", "expected label");

  await extension.awaitMessage("onShown fired");

  // Add new menus, but don't expect them to be rendered yet.
  await extension.callMenuApi("update", "abc", {title: "updated first"});
  await extension.callMenuApi("create", {
    id: "def",
    title: "second",
    contexts,
  });

  elem = extension.getXULElementByMenuId("abc");
  is(elem.getAttribute("label"), "first", "expected unchanged label");
  elem = extension.getXULElementByMenuId("def");
  is(elem, null, "Second menu item should not be visible");

  // Refresh while a menu is shown - should be updated.
  await extension.callMenuApi("refresh");

  elem = extension.getXULElementByMenuId("abc");
  is(elem.getAttribute("label"), "updated first", "expected updated label");
  elem = extension.getXULElementByMenuId("def");
  is(elem.getAttribute("label"), "second", "expected second label");

  // Update the two menu items again.
  await extension.callMenuApi("update", "abc", {enabled: false});
  await extension.callMenuApi("update", "def", {enabled: false});
  await extension.callMenuApi("refresh");
  elem = extension.getXULElementByMenuId("abc");
  is(elem.getAttribute("disabled"), "true", "1st menu item should be disabled");
  elem = extension.getXULElementByMenuId("def");
  is(elem.getAttribute("disabled"), "true", "2nd menu item should be disabled");

  // Remove one.
  await extension.callMenuApi("remove", "abc");
  await extension.callMenuApi("refresh");
  elem = extension.getXULElementByMenuId("def");
  is(elem.getAttribute("label"), "second", "other menu item should exist");
  elem = extension.getXULElementByMenuId("abc");
  is(elem, null, "removed menu item should be gone");

  // Remove the last one.
  await extension.callMenuApi("removeAll");
  await extension.callMenuApi("refresh");
  elem = extension.getXULElementByMenuId("def");
  is(elem, null, "all menu items should be gone");

  // At this point all menu items have been removed. Create a new menu item so
  // we can confirm that browser.menus.refresh() does not render the menu item
  // after the menu has been hidden.
  await extension.callMenuApi("create", {
    // The menu item with ID "abc" was removed before, so re-using the ID should
    // not cause any issues:
    id: "abc",
    title: "re-used",
    contexts,
  });
  await extension.callMenuApi("refresh");
  elem = extension.getXULElementByMenuId("abc");
  is(elem.getAttribute("label"), "re-used", "menu item should be created");

  await doCloseMenu();

  elem = extension.getXULElementByMenuId("abc");
  is(elem, null, "menu item must be gone");

  // Refresh after menu was hidden - should be noop.
  await extension.callMenuApi("refresh");
  elem = extension.getXULElementByMenuId("abc");
  is(elem, null, "menu item must still be gone");

  await extension.unload();
}

add_task(async function refresh_menus_with_browser_action() {
  await testRefreshMenusWhileVisible({
    contexts: ["browser_action"],
    async doOpenMenu(extension) {
      await openActionContextMenu(extension, "browser");
    },
    async doCloseMenu() {
      await closeActionContextMenu();
    },
  });
});

add_task(async function refresh_menus_with_tab() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  await testRefreshMenusWhileVisible({
    contexts: ["tab"],
    async doOpenMenu() {
      await openTabContextMenu();
    },
    async doCloseMenu() {
      await closeTabContextMenu();
    },
  });
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function refresh_menus_with_tools_menu() {
  await testRefreshMenusWhileVisible({
    contexts: ["tools_menu"],
    async doOpenMenu() {
      await openToolsMenu();
    },
    async doCloseMenu() {
      await closeToolsMenu();
    },
  });
});

add_task(async function refresh_menus_with_page() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  await testRefreshMenusWhileVisible({
    contexts: ["page"],
    async doOpenMenu() {
      await openContextMenu("body");
    },
    async doCloseMenu() {
      await closeExtensionContextMenu();
    },
  });
  await BrowserTestUtils.removeTab(tab);
});

// refresh() only works for extensions with at least one visible menu item.
// This test will fail if we ever add support for adding new menu items even if
// the extension has no existing menu item in the shown menu.
add_task(async function refresh_without_menus_at_onShown() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  let extension = loadExtensionWithMenusApi();
  await extension.startup();

  const doOpenMenu = () => openContextMenu("body");
  const doCloseMenu = () => closeExtensionContextMenu();

  await doOpenMenu();
  await extension.callMenuApi("create", {
    id: "too late",
    title: "created after shown",
  });
  await extension.callMenuApi("refresh");
  let elem = extension.getXULElementByMenuId("too late");
  is(elem, null, "extension without visible menu items cannot add new items");

  await extension.callMenuApi("update", "too late", {title: "the menu item"});
  await extension.callMenuApi("refresh");
  elem = extension.getXULElementByMenuId("too late");
  is(elem, null, "updated menu item should still be invisible");

  // The previously created menu item should be visible if the menu is closed
  // and re-opened.
  await doCloseMenu();
  await doOpenMenu();
  await extension.awaitMessage("onShown fired");
  elem = extension.getXULElementByMenuId("too late");
  is(elem.getAttribute("label"), "the menu item", "previously registered item");
  await doCloseMenu();

  await extension.unload();
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function refresh_menus_with_browser_action() {
  let extension = loadExtensionWithMenusApi();
  let other_extension = loadExtensionWithMenusApi();
  await extension.startup();
  await other_extension.startup();

  await extension.callMenuApi("create", {
    id: "action_item",
    title: "visible menu item",
    contexts: ["browser_action"],
  });

  await other_extension.callMenuApi("create", {
    id: "action_item",
    title: "other menu item",
    contexts: ["browser_action"],
  });

  await openActionContextMenu(extension, "browser");
  await extension.awaitMessage("onShown fired");

  let elem = extension.getXULElementByMenuId("action_item");
  is(elem.getAttribute("label"), "visible menu item", "extension menu shown");
  elem = other_extension.getXULElementByMenuId("action_item");
  is(elem, null, "other extension's menu should be hidden");

  await extension.callMenuApi("update", "action_item", {title: "changed"});
  await other_extension.callMenuApi("update", "action_item", {title: "foo"});
  await other_extension.callMenuApi("refresh");

  // refreshing the menu of an unrelated extension should not affect the menu
  // of another extension.
  elem = extension.getXULElementByMenuId("action_item");
  is(elem.getAttribute("label"), "visible menu item", "extension menu shown");
  elem = other_extension.getXULElementByMenuId("action_item");
  is(elem, null, "other extension's menu should be hidden");

  await closeActionContextMenu();
  await extension.unload();
  await other_extension.unload();
});
