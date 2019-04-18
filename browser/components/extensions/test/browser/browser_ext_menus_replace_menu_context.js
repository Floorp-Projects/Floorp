/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function getVisibleChildrenIds(menuElem) {
  return Array.from(menuElem.children).filter(elem => !elem.hidden).map(elem => elem.id || elem.tagName);
}

function checkIsDefaultMenuItemVisible(visibleMenuItemIds) {
  // In this whole test file, we open a menu on a link. Assume that all
  // default menu items are shown if one link-specific menu item is shown.
  ok(visibleMenuItemIds.includes("context-openlink"),
     `The default 'Open Link in New Tab' menu item should be in ${visibleMenuItemIds}.`);
}

// Tests that the context of an extension menu can be changed to:
// - tab
// - bookmark
add_task(async function overrideContext_with_context() {
  // Background script of the main test extension and the auxilary other extension.
  function background() {
    const HTTP_URL = "http://example.com/?SomeTab";
    browser.test.onMessage.addListener(async (msg, tabId) => {
      browser.test.assertEq("testTabAccess", msg, `Expected message in ${browser.runtime.id}`);
      let tab = await browser.tabs.get(tabId);
      if (!tab.url) { // tabs or activeTab not active.
        browser.test.sendMessage("testTabAccessDone", "tab_no_url");
        return;
      }
      try {
        let [url] = await browser.tabs.executeScript(tabId, {
          code: "document.URL",
        });
        browser.test.assertEq(HTTP_URL, url, "Expected successful executeScript");
        browser.test.sendMessage("testTabAccessDone", "executeScript_ok");
        return;
      } catch (e) {
        browser.test.assertEq("Missing host permission for the tab", e.message, "Expected error message");
        browser.test.sendMessage("testTabAccessDone", "executeScript_failed");
      }
    });
    browser.menus.onShown.addListener((info, tab) => {
      browser.test.assertEq("tab", info.viewType, "Expected viewType at onShown");
      browser.test.assertEq(undefined, info.linkUrl, "Expected linkUrl at onShown");
      browser.test.assertEq(undefined, info.srckUrl, "Expected srcUrl at onShown");
      browser.test.sendMessage("onShown", {
        menuIds: info.menuIds.sort(),
        contexts: info.contexts,
        bookmarkId: info.bookmarkId,
        pageUrl: info.pageUrl,
        frameUrl: info.frameUrl,
        tabId: tab && tab.id,
      });
    });
    browser.menus.onClicked.addListener((info, tab) => {
      browser.test.assertEq("tab", info.viewType, "Expected viewType at onClicked");
      browser.test.assertEq(undefined, info.linkUrl, "Expected linkUrl at onClicked");
      browser.test.assertEq(undefined, info.srckUrl, "Expected srcUrl at onClicked");
      browser.test.sendMessage("onClicked", {
        menuItemId: info.menuItemId,
        bookmarkId: info.bookmarkId,
        pageUrl: info.pageUrl,
        frameUrl: info.frameUrl,
        tabId: tab && tab.id,
      });
    });

    // Minimal properties to define menu items for a specific context.
    browser.menus.create({id: "tab_context", title: "tab_context", contexts: ["tab"]});
    browser.menus.create({id: "bookmark_context", title: "bookmark_context", contexts: ["bookmark"]});

    // documentUrlPatterns in the tab context applies to the tab's URL.
    browser.menus.create({id: "tab_context_http", title: "tab_context_http",
                          contexts: ["tab"], documentUrlPatterns: [HTTP_URL]});
    browser.menus.create({id: "tab_context_moz_unexpected", title: "tab_context_moz",
                          contexts: ["tab"], documentUrlPatterns: ["moz-extension://*/tab.html"]});
    // When viewTypes is present, the document's URL is matched instead.
    browser.menus.create({id: "tab_context_viewType_http_unexpected", title: "tab_context_viewType_http",
                          contexts: ["tab"], viewTypes: ["tab"], documentUrlPatterns: [HTTP_URL]});
    browser.menus.create({id: "tab_context_viewType_moz", title: "tab_context_viewType_moz",
                          contexts: ["tab"], viewTypes: ["tab"], documentUrlPatterns: ["moz-extension://*/tab.html"]});

    // documentUrlPatterns is not restricting bookmark menu items.
    browser.menus.create({id: "bookmark_context_http", title: "bookmark_context_http",
                          contexts: ["bookmark"], documentUrlPatterns: [HTTP_URL]});
    browser.menus.create({id: "bookmark_context_moz", title: "bookmark_context_moz",
                          contexts: ["bookmark"], documentUrlPatterns: ["moz-extension://*/tab.html"]});
    // When viewTypes is present, the document's URL is matched instead.
    browser.menus.create({id: "bookmark_context_viewType_http_unexpected", title: "bookmark_context_viewType_http",
                          contexts: ["bookmark"], viewTypes: ["tab"], documentUrlPatterns: [HTTP_URL]});
    browser.menus.create({id: "bookmark_context_viewType_moz", title: "bookmark_context_viewType_moz",
                          contexts: ["bookmark"], viewTypes: ["tab"], documentUrlPatterns: ["moz-extension://*/tab.html"]});

    browser.menus.create({id: "link_context", title: "link_context"}, () => {
      browser.test.sendMessage("menu_items_registered");
    });

    if (browser.runtime.id === "@menu-test-extension") {
      browser.tabs.create({url: "tab.html"});
    }
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id: "@menu-test-extension"}},
      permissions: ["menus", "menus.overrideContext", "tabs", "bookmarks"],
    },
    files: {
      "tab.html": `
        <!DOCTYPE html><meta charset="utf-8">
        <a href="http://example.com/">Link</a>
        <script src="tab.js"></script>
      `,
      "tab.js": async () => {
        let [tab] = await browser.tabs.query({
          url: "http://example.com/?SomeTab",
        });
        let bookmark = await browser.bookmarks.create({
          title: "Bookmark for menu test",
          url: "http://example.com/bookmark",
        });
        let testCases = [{
          context: "tab",
          tabId: tab.id,
        }, {
          context: "tab",
          tabId: tab.id,
        }, {
          context: "bookmark",
          bookmarkId: bookmark.id,
        }, {
          context: "tab",
          tabId: 123456789, // Some invalid tabId.
        }];

        // eslint-disable-next-line mozilla/balanced-listeners
        document.addEventListener("contextmenu", () => {
          browser.menus.overrideContext(testCases.shift());
          browser.test.sendMessage("oncontextmenu_in_dom");
        });

        browser.test.sendMessage("setup_ready", {
          bookmarkId: bookmark.id,
          tabId: tab.id,
          httpUrl: tab.url,
          extensionUrl: document.URL,
        });
      },
    },
    background,
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/?SomeTab");

  let otherExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id: "@other-test-extension"}},
      permissions: ["menus", "bookmarks", "activeTab"],
    },
    background,
  });
  await otherExtension.startup();
  await otherExtension.awaitMessage("menu_items_registered");

  await extension.startup();
  await extension.awaitMessage("menu_items_registered");

  let {bookmarkId, tabId, httpUrl, extensionUrl} = await extension.awaitMessage("setup_ready");
  info(`Set up test with tabId=${tabId} and bookmarkId=${bookmarkId}.`);

  {
    // Test case 1: context=tab
    let menu = await openContextMenu("a");
    await extension.awaitMessage("oncontextmenu_in_dom");
    for (let ext of [extension, otherExtension]) {
      info(`Testing menu from ${ext.id} after changing context to tab`);
      Assert.deepEqual(await ext.awaitMessage("onShown"), {
        menuIds: [
          "tab_context",
          "tab_context_http",
          "tab_context_viewType_moz",
        ],
        contexts: ["tab"],
        bookmarkId: undefined,
        pageUrl: undefined, // because extension has no host permissions.
        frameUrl: extensionUrl,
        tabId,
      }, "Expected onShown details after changing context to tab");
    }
    let topLevels = menu.getElementsByAttribute("ext-type", "top-level-menu");
    is(topLevels.length, 1, "Expected top-level menu for otherExtension");

    Assert.deepEqual(getVisibleChildrenIds(menu), [
      `${makeWidgetId(extension.id)}-menuitem-_tab_context`,
      `${makeWidgetId(extension.id)}-menuitem-_tab_context_http`,
      `${makeWidgetId(extension.id)}-menuitem-_tab_context_viewType_moz`,
      `menuseparator`,
      topLevels[0].id,
    ], "Expected menu items after changing context to tab");

    let submenu = await openSubmenu(topLevels[0]);
    is(submenu, topLevels[0].menupopup, "Correct submenu opened");

    Assert.deepEqual(getVisibleChildrenIds(submenu), [
      `${makeWidgetId(otherExtension.id)}-menuitem-_tab_context`,
      `${makeWidgetId(otherExtension.id)}-menuitem-_tab_context_http`,
      `${makeWidgetId(otherExtension.id)}-menuitem-_tab_context_viewType_moz`,
    ], "Expected menu items in submenu after changing context to tab");

    extension.sendMessage("testTabAccess", tabId);
    is(await extension.awaitMessage("testTabAccessDone"),
       "executeScript_failed",
       "executeScript should fail due to the lack of permissions.");

    otherExtension.sendMessage("testTabAccess", tabId);
    is(await otherExtension.awaitMessage("testTabAccessDone"),
       "tab_no_url",
       "Other extension should not have activeTab permissions yet.");

    // Click on the menu item of the other extension to unlock host permissions.
    let menuItems = menu.getElementsByAttribute("label", "tab_context");
    is(menuItems.length, 2, "There are two menu items with label 'tab_context'");
    await closeExtensionContextMenu(menuItems[1]);

    Assert.deepEqual(await otherExtension.awaitMessage("onClicked"), {
      menuItemId: "tab_context",
      bookmarkId: undefined,
      pageUrl: httpUrl,
      frameUrl: extensionUrl,
      tabId,
    }, "Expected onClicked details after changing context to tab");

    extension.sendMessage("testTabAccess", tabId);
    is(await extension.awaitMessage("testTabAccessDone"),
       "executeScript_failed",
       "executeScript of extension that created the menu should still fail.");

    otherExtension.sendMessage("testTabAccess", tabId);
    is(await otherExtension.awaitMessage("testTabAccessDone"),
       "executeScript_ok",
       "Other extension should have activeTab permissions.");
  }

  {
    // Test case 2: context=tab, click on menu item of extension..
    let menu = await openContextMenu("a");
    await extension.awaitMessage("oncontextmenu_in_dom");

    // The previous test has already verified the visible menu items,
    // so we skip checking the onShown result and only test clicking.
    await extension.awaitMessage("onShown");
    await otherExtension.awaitMessage("onShown");
    let menuItems = menu.getElementsByAttribute("label", "tab_context");
    is(menuItems.length, 2, "There are two menu items with label 'tab_context'");
    await closeExtensionContextMenu(menuItems[0]);

    Assert.deepEqual(await extension.awaitMessage("onClicked"), {
      menuItemId: "tab_context",
      bookmarkId: undefined,
      pageUrl: httpUrl,
      frameUrl: extensionUrl,
      tabId,
    }, "Expected onClicked details after changing context to tab");

    extension.sendMessage("testTabAccess", tabId);
    is(await extension.awaitMessage("testTabAccessDone"),
       "executeScript_failed",
       "activeTab permission should not be available to the extension that created the menu.");
  }

  {
    // Test case 3: context=bookmark
    let menu = await openContextMenu("a");
    await extension.awaitMessage("oncontextmenu_in_dom");
    for (let ext of [extension, otherExtension]) {
      info(`Testing menu from ${ext.id} after changing context to bookmark`);
      let shownInfo = await ext.awaitMessage("onShown");
      Assert.deepEqual(shownInfo, {
        menuIds: [
          "bookmark_context",
          "bookmark_context_http",
          "bookmark_context_moz",
          "bookmark_context_viewType_moz",
        ],
        contexts: ["bookmark"],
        bookmarkId,
        pageUrl: undefined,
        frameUrl: extensionUrl,
        tabId: undefined,
      }, "Expected onShown details after changing context to bookmark");
    }
    let topLevels = menu.getElementsByAttribute("ext-type", "top-level-menu");
    is(topLevels.length, 1, "Expected top-level menu for otherExtension");

    Assert.deepEqual(getVisibleChildrenIds(menu), [
      `${makeWidgetId(extension.id)}-menuitem-_bookmark_context`,
      `${makeWidgetId(extension.id)}-menuitem-_bookmark_context_http`,
      `${makeWidgetId(extension.id)}-menuitem-_bookmark_context_moz`,
      `${makeWidgetId(extension.id)}-menuitem-_bookmark_context_viewType_moz`,
      `menuseparator`,
      topLevels[0].id,
    ], "Expected menu items after changing context to bookmark");

    let submenu = await openSubmenu(topLevels[0]);
    is(submenu, topLevels[0].menupopup, "Correct submenu opened");

    Assert.deepEqual(getVisibleChildrenIds(submenu), [
      `${makeWidgetId(otherExtension.id)}-menuitem-_bookmark_context`,
      `${makeWidgetId(otherExtension.id)}-menuitem-_bookmark_context_http`,
      `${makeWidgetId(otherExtension.id)}-menuitem-_bookmark_context_moz`,
      `${makeWidgetId(otherExtension.id)}-menuitem-_bookmark_context_viewType_moz`,
    ], "Expected menu items in submenu after changing context to bookmark");
    await closeContextMenu(menu);
  }

  {
    // Test case 4: context=tab, invalid tabId.
    let menu = await openContextMenu("a");
    await extension.awaitMessage("oncontextmenu_in_dom");
    // When an invalid tabId is used, all extension menu logic is skipped and
    // the default menu is shown.
    checkIsDefaultMenuItemVisible(getVisibleChildrenIds(menu));
    await closeContextMenu(menu);
  }

  await extension.unload();
  await otherExtension.unload();
  BrowserTestUtils.removeTab(tab);
});
