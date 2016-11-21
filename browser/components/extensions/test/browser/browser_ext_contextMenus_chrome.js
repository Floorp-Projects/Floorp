/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(function* test_actionContextMenus() {
  const manifest = {
    page_action: {},
    browser_action: {},
    permissions: ["contextMenus"],
  };

  async function background() {
    const contexts = ["page_action", "browser_action"];

    const parentId = browser.contextMenus.create({contexts, title: "parent"});
    await browser.contextMenus.create({contexts, parentId, title: "click A"});
    await browser.contextMenus.create({contexts, parentId, title: "click B"});

    for (let i = 1; i < 9; i++) {
      await browser.contextMenus.create({contexts, title: `click ${i}`});
    }

    browser.contextMenus.onClicked.addListener((info, tab) => {
      browser.test.sendMessage("click", {info, tab});
    });

    const [tab] = await browser.tabs.query({active: true});
    await browser.pageAction.show(tab.id);
    browser.test.sendMessage("ready", tab.id);
  }

  const extension = ExtensionTestUtils.loadExtension({manifest, background});
  const tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

  yield extension.startup();
  const tabId = yield extension.awaitMessage("ready");

  for (const kind of ["page", "browser"]) {
    const menu = yield openActionContextMenu(extension, kind);
    const [submenu, second, , , , last, separator] = menu.children;

    is(submenu.tagName, "menu", "Correct submenu type");
    is(submenu.label, "parent", "Correct submenu title");

    const popup = yield openSubmenu(submenu);
    is(popup, submenu.firstChild, "Correct submenu opened");
    is(popup.children.length, 2, "Correct number of submenu items");

    is(second.tagName, "menuitem", "Second menu item type is correct");
    is(second.label, "click 1", "Second menu item title is correct");

    is(last.label, "click 5", "Last menu item title is correct");
    is(separator.tagName, "menuseparator", "Separator after last menu item");

    yield closeActionContextMenu(popup.firstChild);
    const {info, tab} = yield extension.awaitMessage("click");
    is(info.pageUrl, "http://example.com/", "Click info pageUrl is correct");
    is(tab.id, tabId, "Click event tab ID is correct");
  }

  yield BrowserTestUtils.removeTab(tab);
  yield extension.unload();
});

add_task(function* test_tabContextMenu() {
  const first = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["contextMenus"],
    },
    async background() {
      await browser.contextMenus.create({title: "alpha", contexts: ["tab"]});
      await browser.contextMenus.create({title: "beta", contexts: ["tab"]});

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

  const tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  yield first.startup();
  yield second.startup();

  const tabId = yield first.awaitMessage("ready");
  yield second.awaitMessage("ready");

  const menu = yield openTabContextMenu();
  const [separator, submenu, gamma] = Array.from(menu.children).slice(-3);
  is(separator.tagName, "menuseparator", "Separator before first extension item");

  is(submenu.tagName, "menu", "Correct submenu type");
  is(submenu.label, "Generated extension", "Correct submenu title");

  is(gamma.tagName, "menuitem", "Third menu item type is correct");
  is(gamma.label, "gamma", "Third menu item label is correct");

  const popup = yield openSubmenu(submenu);
  is(popup, submenu.firstChild, "Correct submenu opened");
  is(popup.children.length, 2, "Correct number of submenu items");

  const [alpha, beta] = popup.children;
  is(alpha.tagName, "menuitem", "First menu item type is correct");
  is(alpha.label, "alpha", "First menu item label is correct");
  is(beta.tagName, "menuitem", "Second menu item type is correct");
  is(beta.label, "beta", "Second menu item label is correct");

  yield closeTabContextMenu(beta);
  const click = yield first.awaitMessage("click");
  is(click.info.pageUrl, "http://example.com/", "Click info pageUrl is correct");
  is(click.tab.id, tabId, "Click event tab ID is correct");

  yield BrowserTestUtils.removeTab(tab);
  yield first.unload();
  yield second.unload();
});
