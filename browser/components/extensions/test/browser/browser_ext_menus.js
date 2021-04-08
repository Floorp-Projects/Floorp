/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const PAGE =
  "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";

async function openContextMenuInPageActionPanel(extension, win = window) {
  win.gURLBar.setPageProxyState("valid");
  await promiseAnimationFrame(win);
  const mainPanelshown = BrowserTestUtils.waitForEvent(
    BrowserPageActions.panelNode,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(
    BrowserPageActions.mainButtonNode,
    {},
    win
  );
  await mainPanelshown;
  let buttonID =
    "#" +
    BrowserPageActions.panelButtonNodeIDForActionID(makeWidgetId(extension.id));
  let menuID = "pageActionContextMenu";
  return openChromeContextMenu(menuID, buttonID, win);
}

add_task(async function test_permissions() {
  function background() {
    browser.test.sendMessage("apis", {
      menus: typeof browser.menus,
      contextMenus: typeof browser.contextMenus,
      menusInternal: typeof browser.menusInternal,
    });
  }

  const first = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["menus"] },
    background,
  });
  const second = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["contextMenus"] },
    background,
  });

  await first.startup();
  await second.startup();

  const apis1 = await first.awaitMessage("apis");
  const apis2 = await second.awaitMessage("apis");

  is(apis1.menus, "object", "browser.menus available with 'menus' permission");
  is(
    apis1.contextMenus,
    "undefined",
    "browser.contextMenus unavailable with  'menus' permission"
  );
  is(
    apis1.menusInternal,
    "undefined",
    "browser.menusInternal is never available"
  );

  is(
    apis2.menus,
    "undefined",
    "browser.menus unavailable with 'contextMenus' permission"
  );
  is(
    apis2.contextMenus,
    "object",
    "browser.contextMenus unavailable with  'contextMenus' permission"
  );
  is(
    apis2.menusInternal,
    "undefined",
    "browser.menusInternal is never available"
  );

  await first.unload();
  await second.unload();
});

add_task(async function test_actionContextMenus() {
  const manifest = {
    page_action: {},
    browser_action: {},
    permissions: ["menus"],
  };

  async function background() {
    const contexts = ["page_action", "browser_action"];

    const parentId = browser.menus.create({ contexts, title: "parent" });
    browser.menus.create({ parentId, title: "click A" });
    browser.menus.create({ parentId, title: "click B" });

    for (let i = 1; i < 9; i++) {
      browser.menus.create({ contexts, id: `${i}`, title: `click ${i}` });
    }

    browser.menus.onClicked.addListener((info, tab) => {
      browser.test.sendMessage("click", { info, tab });
    });

    const [tab] = await browser.tabs.query({ active: true });
    await browser.pageAction.show(tab.id);
    browser.test.sendMessage("ready", tab.id);
  }

  const extension = ExtensionTestUtils.loadExtension({ manifest, background });
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/"
  );

  await extension.startup();
  const tabId = await extension.awaitMessage("ready");

  for (const kind of ["page", "browser"]) {
    const menu = await openActionContextMenu(extension, kind);
    const [submenu, second, , , , last, separator] = menu.children;

    is(submenu.tagName, "menu", "Correct submenu type");
    is(submenu.label, "parent", "Correct submenu title");

    const popup = await openSubmenu(submenu);
    is(popup, submenu.menupopup, "Correct submenu opened");
    is(popup.children.length, 2, "Correct number of submenu items");

    let idPrefix = `${makeWidgetId(extension.id)}-menuitem-_`;

    is(second.tagName, "menuitem", "Second menu item type is correct");
    is(second.label, "click 1", "Second menu item title is correct");
    is(second.id, `${idPrefix}1`, "Second menu item id is correct");

    is(last.tagName, "menu", "Last menu item type is correct");
    is(last.label, "Generated extension", "Last menu item title is correct");
    is(
      last.getAttribute("ext-type"),
      "top-level-menu",
      "Last menu ext-type is correct"
    );
    is(separator.tagName, "menuseparator", "Separator after last menu item");

    // Verify that menu items exceeding ACTION_MENU_TOP_LEVEL_LIMIT are moved into a submenu.
    let overflowPopup = await openSubmenu(last);
    is(
      overflowPopup.children.length,
      4,
      "Excess items should be moved into a submenu"
    );
    is(
      overflowPopup.firstElementChild.id,
      `${idPrefix}5`,
      "First submenu item ID is correct"
    );
    is(
      overflowPopup.lastElementChild.id,
      `${idPrefix}8`,
      "Last submenu item ID is correct"
    );

    await closeActionContextMenu(overflowPopup.firstElementChild, kind);
    const { info, tab } = await extension.awaitMessage("click");
    is(info.pageUrl, "http://example.com/", "Click info pageUrl is correct");
    is(tab.id, tabId, "Click event tab ID is correct");
  }

  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function test_hiddenPageActionContextMenu() {
  // In Proton there's no page actions menu by default, there is only an
  // overflow menu when the window is smaller than 680px. While currently
  // disabled page actions are shown in that overflow menu, bug 1703889 will
  // change the behavior, hiding them. Then this test won't be necessary anymore
  // since the user won't be able to open the context menu on disabled actions.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.proton.urlbar.enabled", false]],
  });
  const manifest = {
    page_action: {},
    permissions: ["menus"],
  };

  async function background() {
    const contexts = ["page_action"];

    const parentId = browser.menus.create({ contexts, title: "parent" });
    browser.menus.create({ parentId, title: "click A" });
    browser.menus.create({ parentId, title: "click B" });

    for (let i = 1; i < 9; i++) {
      browser.menus.create({ contexts, id: `${i}`, title: `click ${i}` });
    }

    const [tab] = await browser.tabs.query({ active: true });
    await browser.pageAction.hide(tab.id);
    browser.test.sendMessage("ready", tab.id);
  }

  const extension = ExtensionTestUtils.loadExtension({ manifest, background });
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/"
  );

  await extension.startup();
  await extension.awaitMessage("ready");

  const menu = await openContextMenuInPageActionPanel(extension);
  const menuItems = Array.prototype.filter.call(menu.children, node => {
    return window.getComputedStyle(node).visibility == "visible";
  });

  is(menuItems.length, 4, "Correct number of children");
  const [dontShowItem, separator, manageItem, removeItem] = menuItems;

  is(dontShowItem.label, "Remove from Address Bar", "Correct first child");
  is(separator.tagName, "menuseparator", "Correct second child");
  is(manageItem.label, "Manage Extension\u2026", "Correct third child");
  is(removeItem.label, "Remove Extension", "Correct fourth child");

  await closeChromeContextMenu(menu.id);
  await closeChromeContextMenu(BrowserPageActions.panelNode.id);

  // Undo the Proton pref change.
  await SpecialPowers.popPrefEnv();
  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function test_bookmarkContextMenu() {
  const ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus", "bookmarks"],
    },
    background() {
      browser.menus.onShown.addListener(() => {
        browser.test.sendMessage("hello");
      });
      browser.menus.create({ title: "blarg", contexts: ["bookmark"] }, () => {
        browser.test.sendMessage("ready");
      });
    },
  });

  await toggleBookmarksToolbar(true);
  await ext.startup();
  await ext.awaitMessage("ready");

  let menu = await openChromeContextMenu(
    "placesContext",
    "#PlacesToolbarItems .bookmark-item"
  );
  let children = Array.from(menu.children);
  let item = children[children.length - 1];
  is(item.label, "blarg", "Menu item label is correct");
  await ext.awaitMessage("hello"); // onShown listener fired

  closeChromeContextMenu("placesContext", item);
  await ext.unload();
  await toggleBookmarksToolbar(false);
});

add_task(async function test_tabContextMenu() {
  const first = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    async background() {
      browser.menus.create({
        id: "alpha-beta-parent",
        title: "alpha-beta parent",
        contexts: ["tab"],
      });

      browser.menus.create({ parentId: "alpha-beta-parent", title: "alpha" });
      browser.menus.create({ parentId: "alpha-beta-parent", title: "beta" });

      browser.menus.create({ title: "dummy", contexts: ["page"] });

      browser.menus.onClicked.addListener((info, tab) => {
        browser.test.sendMessage("click", { info, tab });
      });

      const [tab] = await browser.tabs.query({ active: true });
      browser.test.sendMessage("ready", tab.id);
    },
  });

  const second = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    background() {
      browser.menus.create({
        title: "invisible",
        contexts: ["tab"],
        documentUrlPatterns: ["http://does/not/match"],
      });
      browser.menus.create(
        {
          title: "gamma",
          contexts: ["tab"],
          documentUrlPatterns: ["http://example.com/"],
        },
        () => {
          browser.test.sendMessage("ready");
        }
      );
    },
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/"
  );
  await first.startup();
  await second.startup();

  const tabId = await first.awaitMessage("ready");
  await second.awaitMessage("ready");

  const menu = await openTabContextMenu();
  const [separator, submenu, gamma] = Array.from(menu.children).slice(-3);
  is(
    separator.tagName,
    "menuseparator",
    "Separator before first extension item"
  );

  is(submenu.tagName, "menu", "Correct submenu type");
  is(submenu.label, "alpha-beta parent", "Correct submenu title");

  isnot(
    gamma.label,
    "dummy",
    "`page` context menu item should not appear here"
  );

  is(gamma.tagName, "menuitem", "Third menu item type is correct");
  is(gamma.label, "gamma", "Third menu item label is correct");

  const popup = await openSubmenu(submenu);
  is(popup, submenu.menupopup, "Correct submenu opened");
  is(popup.children.length, 2, "Correct number of submenu items");

  const [alpha, beta] = popup.children;
  is(alpha.tagName, "menuitem", "First menu item type is correct");
  is(alpha.label, "alpha", "First menu item label is correct");
  is(beta.tagName, "menuitem", "Second menu item type is correct");
  is(beta.label, "beta", "Second menu item label is correct");

  await closeTabContextMenu(beta);
  const click = await first.awaitMessage("click");
  is(
    click.info.pageUrl,
    "http://example.com/",
    "Click info pageUrl is correct"
  );
  is(click.tab.id, tabId, "Click event tab ID is correct");
  is(click.info.frameId, undefined, "no frameId on chrome");

  BrowserTestUtils.removeTab(tab);
  await first.unload();
  await second.unload();
});

add_task(async function test_onclick_frameid() {
  const manifest = {
    permissions: ["menus"],
  };

  function background() {
    function onclick(info) {
      browser.test.sendMessage("click", info);
    }
    browser.menus.create(
      { contexts: ["frame", "page"], title: "modify", onclick },
      () => {
        browser.test.sendMessage("ready");
      }
    );
  }

  const extension = ExtensionTestUtils.loadExtension({ manifest, background });
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  await extension.startup();
  await extension.awaitMessage("ready");

  async function click(menu) {
    const items = menu.getElementsByAttribute("label", "modify");
    is(items.length, 1, "found menu item");
    await closeExtensionContextMenu(items[0]);
    return extension.awaitMessage("click");
  }

  let info = await click(await openContextMenu("body"));
  is(info.frameId, 0, "top level click");
  info = await click(await openContextMenuInFrame());
  isnot(info.frameId, undefined, "frame click, frameId is not undefined");
  isnot(info.frameId, 0, "frame click, frameId probably okay");

  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function test_multiple_contexts_init() {
  const manifest = {
    permissions: ["menus"],
  };

  function background() {
    browser.menus.create({ id: "parent", title: "parent" }, () => {
      browser.tabs.create({ url: "tab.html", active: false });
    });
  }

  const files = {
    "tab.html":
      "<!DOCTYPE html><meta charset=utf-8><script src=tab.js></script>",
    "tab.js": function() {
      browser.menus.onClicked.addListener(info => {
        browser.test.sendMessage("click", info);
      });
      browser.menus.create(
        { parentId: "parent", id: "child", title: "child" },
        () => {
          browser.test.sendMessage("ready");
        }
      );
    },
  };

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  const extension = ExtensionTestUtils.loadExtension({
    manifest,
    background,
    files,
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  const menu = await openContextMenu();
  const items = menu.getElementsByAttribute("label", "parent");

  is(items.length, 1, "Found parent menu item");
  is(items[0].tagName, "menu", "And it has children");

  const popup = await openSubmenu(items[0]);
  is(popup.firstElementChild.label, "child", "Correct child menu item");
  await closeExtensionContextMenu(popup.firstElementChild);

  const info = await extension.awaitMessage("click");
  is(info.menuItemId, "child", "onClicked the correct item");

  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function test_tools_menu() {
  const first = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    background() {
      browser.menus.create({ title: "alpha", contexts: ["tools_menu"] });
      browser.menus.create({ title: "beta", contexts: ["tools_menu"] }, () => {
        browser.test.sendMessage("ready");
      });
    },
  });

  const second = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    async background() {
      browser.menus.create({ title: "gamma", contexts: ["tools_menu"] });
      browser.menus.onClicked.addListener((info, tab) => {
        browser.test.sendMessage("click", { info, tab });
      });

      const [tab] = await browser.tabs.query({ active: true });
      browser.test.sendMessage("ready", tab.id);
    },
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/"
  );
  await first.startup();
  await second.startup();

  await first.awaitMessage("ready");
  const tabId = await second.awaitMessage("ready");
  const menu = await openToolsMenu();

  const [separator, submenu, gamma] = Array.from(menu.children).slice(-3);
  is(
    separator.tagName,
    "menuseparator",
    "Separator before first extension item"
  );

  is(submenu.tagName, "menu", "Correct submenu type");
  is(
    submenu.getAttribute("label"),
    "Generated extension",
    "Correct submenu title"
  );
  is(submenu.menupopup.children.length, 2, "Correct number of submenu items");

  is(gamma.tagName, "menuitem", "Third menu item type is correct");
  is(gamma.getAttribute("label"), "gamma", "Third menu item label is correct");

  closeToolsMenu(gamma);

  const click = await second.awaitMessage("click");
  is(
    click.info.pageUrl,
    "http://example.com/",
    "Click info pageUrl is correct"
  );
  is(click.tab.id, tabId, "Click event tab ID is correct");

  BrowserTestUtils.removeTab(tab);
  await first.unload();
  await second.unload();
});
