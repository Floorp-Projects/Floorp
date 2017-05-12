/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function test_actionContextMenus() {
  const manifest = {
    page_action: {},
    browser_action: {},
    permissions: ["contextMenus"],
  };

  async function background() {
    const contexts = ["page_action", "browser_action"];

    const parentId = browser.contextMenus.create({contexts, title: "parent"});
    await browser.contextMenus.create({parentId, title: "click A"});
    await browser.contextMenus.create({parentId, title: "click B"});

    for (let i = 1; i < 9; i++) {
      await browser.contextMenus.create({contexts, id: `${i}`, title: `click ${i}`});
    }

    browser.contextMenus.onClicked.addListener((info, tab) => {
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
      permissions: ["contextMenus"],
    },
    async background() {
      await browser.contextMenus.create({
        id: "alpha-beta-parent", title: "alpha-beta parent", contexts: ["tab"],
      });

      await browser.contextMenus.create({parentId: "alpha-beta-parent", title: "alpha"});
      await browser.contextMenus.create({parentId: "alpha-beta-parent", title: "beta"});

      await browser.contextMenus.create({title: "dummy", contexts: ["page"]});

      browser.contextMenus.onClicked.addListener((info, tab) => {
        browser.test.sendMessage("click", {info, tab});
      });

      const [tab] = await browser.tabs.query({active: true});
      browser.test.sendMessage("ready", tab.id);
    },
  });

  const second = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["contextMenus"],
    },
    async background() {
      await browser.contextMenus.create({title: "gamma", contexts: ["tab"]});
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
    permissions: ["contextMenus"],
  };

  function background() {
    function onclick(info) {
      browser.test.sendMessage("click", info);
    }
    browser.contextMenus.create({contexts: ["frame", "page"], title: "modify", onclick});
    browser.test.sendMessage("ready");
  }

  const extension = ExtensionTestUtils.loadExtension({manifest, background});
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser,
    "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html");

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
