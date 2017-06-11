/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const PAGE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";

add_task(async function test_permissions() {
  function background() {
    browser.test.sendMessage("apis", {
      menus: typeof browser.menus,
      contextMenus: typeof browser.contextMenus,
      menusInternal: typeof browser.menusInternal,
    });
  }

  const first = ExtensionTestUtils.loadExtension({manifest: {permissions: ["menus"]}, background});
  const second = ExtensionTestUtils.loadExtension({manifest: {permissions: ["contextMenus"]}, background});

  await first.startup();
  await second.startup();

  const apis1 = await first.awaitMessage("apis");
  const apis2 = await second.awaitMessage("apis");

  is(apis1.menus, "object", "browser.menus available with 'menus' permission");
  is(apis1.contextMenus, "undefined", "browser.contextMenus unavailable with  'menus' permission");
  is(apis1.menusInternal, "undefined", "browser.menusInternal is never available");

  is(apis2.menus, "undefined", "browser.menus unavailable with 'contextMenus' permission");
  is(apis2.contextMenus, "object", "browser.contextMenus unavailable with  'contextMenus' permission");
  is(apis2.menusInternal, "undefined", "browser.menusInternal is never available");

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

    const parentId = browser.menus.create({contexts, title: "parent"});
    await browser.menus.create({parentId, title: "click A"});
    await browser.menus.create({parentId, title: "click B"});

    for (let i = 1; i < 9; i++) {
      await browser.menus.create({contexts, id: `${i}`, title: `click ${i}`});
    }

    browser.menus.onClicked.addListener((info, tab) => {
      browser.test.sendMessage("click", {info, tab});
    });

    const [tab] = await browser.tabs.query({active: true});
    await browser.pageAction.show(tab.id);
    browser.test.sendMessage("ready", tab.id);
  }

  const extension = ExtensionTestUtils.loadExtension({manifest, background});
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

  await extension.startup();
  const tabId = await extension.awaitMessage("ready");

  for (const kind of ["page", "browser"]) {
    const menu = await openActionContextMenu(extension, kind);
    const [submenu, second, , , , last, separator] = menu.children;

    is(submenu.tagName, "menu", "Correct submenu type");
    is(submenu.label, "parent", "Correct submenu title");

    const popup = await openSubmenu(submenu);
    is(popup, submenu.firstChild, "Correct submenu opened");
    is(popup.children.length, 2, "Correct number of submenu items");

    let idPrefix = `${makeWidgetId(extension.id)}_`;

    is(second.tagName, "menuitem", "Second menu item type is correct");
    is(second.label, "click 1", "Second menu item title is correct");
    is(second.id, `${idPrefix}1`, "Second menu item id is correct");

    is(last.label, "click 5", "Last menu item title is correct");
    is(last.id, `${idPrefix}5`, "Last menu item id is correct");
    is(separator.tagName, "menuseparator", "Separator after last menu item");

    await closeActionContextMenu(popup.firstChild);
    const {info, tab} = await extension.awaitMessage("click");
    is(info.pageUrl, "http://example.com/", "Click info pageUrl is correct");
    is(tab.id, tabId, "Click event tab ID is correct");
  }

  await BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function test_tabContextMenu() {
  const first = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    async background() {
      await browser.menus.create({
        id: "alpha-beta-parent", title: "alpha-beta parent", contexts: ["tab"],
      });

      await browser.menus.create({parentId: "alpha-beta-parent", title: "alpha"});
      await browser.menus.create({parentId: "alpha-beta-parent", title: "beta"});

      await browser.menus.create({title: "dummy", contexts: ["page"]});

      browser.menus.onClicked.addListener((info, tab) => {
        browser.test.sendMessage("click", {info, tab});
      });

      const [tab] = await browser.tabs.query({active: true});
      browser.test.sendMessage("ready", tab.id);
    },
  });

  const second = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    async background() {
      await browser.menus.create({title: "gamma", contexts: ["tab"]});
      browser.test.sendMessage("ready");
    },
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  await first.startup();
  await second.startup();

  const tabId = await first.awaitMessage("ready");
  await second.awaitMessage("ready");

  const menu = await openTabContextMenu();
  const [separator, submenu, gamma] = Array.from(menu.children).slice(-3);
  is(separator.tagName, "menuseparator", "Separator before first extension item");

  is(submenu.tagName, "menu", "Correct submenu type");
  is(submenu.label, "alpha-beta parent", "Correct submenu title");

  isnot(gamma.label, "dummy", "`page` context menu item should not appear here");

  is(gamma.tagName, "menuitem", "Third menu item type is correct");
  is(gamma.label, "gamma", "Third menu item label is correct");

  const popup = await openSubmenu(submenu);
  is(popup, submenu.firstChild, "Correct submenu opened");
  is(popup.children.length, 2, "Correct number of submenu items");

  const [alpha, beta] = popup.children;
  is(alpha.tagName, "menuitem", "First menu item type is correct");
  is(alpha.label, "alpha", "First menu item label is correct");
  is(beta.tagName, "menuitem", "Second menu item type is correct");
  is(beta.label, "beta", "Second menu item label is correct");

  await closeTabContextMenu(beta);
  const click = await first.awaitMessage("click");
  is(click.info.pageUrl, "http://example.com/", "Click info pageUrl is correct");
  is(click.tab.id, tabId, "Click event tab ID is correct");
  is(click.info.frameId, undefined, "no frameId on chrome");

  await BrowserTestUtils.removeTab(tab);
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
    browser.menus.create({contexts: ["frame", "page"], title: "modify", onclick});
    browser.test.sendMessage("ready");
  }

  const extension = ExtensionTestUtils.loadExtension({manifest, background});
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  await extension.startup();
  await extension.awaitMessage("ready");

  async function click(selectorOrId) {
    const func = (selectorOrId == "body") ? openContextMenu : openContextMenuInFrame;
    const menu = await func(selectorOrId);
    const items = menu.getElementsByAttribute("label", "modify");
    await closeExtensionContextMenu(items[0]);
    return extension.awaitMessage("click");
  }

  let info = await click("body");
  is(info.frameId, 0, "top level click");
  info = await click("frame");
  isnot(info.frameId, undefined, "frame click, frameId is not undefined");
  isnot(info.frameId, 0, "frame click, frameId probably okay");

  await BrowserTestUtils.removeTab(tab);
  await extension.unload();
});

add_task(async function test_multiple_contexts_init() {
  const manifest = {
    permissions: ["menus"],
  };

  function background() {
    browser.menus.create({id: "parent", title: "parent"});
    browser.tabs.create({url: "tab.html", active: false});
  }

  const files = {
    "tab.html": "<!DOCTYPE html><meta charset=utf-8><script src=tab.js></script>",
    "tab.js": function() {
      browser.menus.create({parentId: "parent", id: "child", title: "child"});

      browser.menus.onClicked.addListener(info => {
        browser.test.sendMessage("click", info);
      });

      browser.test.sendMessage("ready");
    },
  };

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  const extension = ExtensionTestUtils.loadExtension({manifest, background, files});

  await extension.startup();
  await extension.awaitMessage("ready");

  const menu = await openContextMenu();
  const items = menu.getElementsByAttribute("label", "parent");

  is(items.length, 1, "Found parent menu item");
  is(items[0].tagName, "menu", "And it has children");

  const popup = await openSubmenu(items[0]);
  is(popup.firstChild.label, "child", "Correct child menu item");
  await closeExtensionContextMenu(popup.firstChild);

  const info = await extension.awaitMessage("click");
  is(info.menuItemId, "child", "onClicked the correct item");

  await BrowserTestUtils.removeTab(tab);
  await extension.unload();
});
