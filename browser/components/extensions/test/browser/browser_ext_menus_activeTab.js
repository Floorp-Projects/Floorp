/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Opens two tabs at the start of the tab strip and focuses the second tab.
// Then an extension menu is registered for the "tab" context and a menu is
// opened on the first tab and the extension menu item is clicked.
// This triggers the onTabMenuClicked handler.
async function openTwoTabsAndOpenTabMenu(onTabMenuClicked) {
  const PAGE_URL = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";
  const OTHER_URL = "http://127.0.0.1:8888/browser/browser/components/extensions/test/browser/context.html";

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, OTHER_URL);
  // Move the first tab to the start so that it can be found by the .tabbrowser-tab selector below.
  gBrowser.moveTabTo(tab1, 0);
  gBrowser.moveTabTo(tab2, 1);

  async function background(onTabMenuClicked) {
    browser.menus.onClicked.addListener(async (info, tab) => {
      await onTabMenuClicked(info, tab);
      browser.test.sendMessage("onCommand_on_tab_click");
    });

    browser.menus.create({
      title: "menu item on tab",
      contexts: ["tab"],
    }, () => {
      browser.test.sendMessage("ready");
    });
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["menus", "activeTab"],
    },
    background: `(${background})(${onTabMenuClicked})`,
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  // Focus a selected tab to to make tabbrowser.js to load localization files,
  // and thereby initialize document.l10n property.
  gBrowser.selectedTab.focus();

  // The .tabbrowser-tab selector matches the first tab (tab1).
  let menu = await openChromeContextMenu("tabContextMenu", ".tabbrowser-tab", window);
  let menuItem = menu.getElementsByAttribute("label", "menu item on tab")[0];
  await closeTabContextMenu(menuItem);
  await extension.awaitMessage("onCommand_on_tab_click");

  await extension.unload();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
}

add_task(async function activeTabForTabMenu() {
  await openTwoTabsAndOpenTabMenu(async function onTabMenuClicked(info, tab) {
    browser.test.assertEq(0, tab.index, "Expected a menu on the first tab.");

    try {
      let [actualUrl] = await browser.tabs.executeScript(tab.id, {code: "document.URL"});
      browser.test.assertEq(tab.url, actualUrl, "Content script to execute in the first tab");
      // (the activeTab permission should have been granted to the first tab.)
    } catch (e) {
      browser.test.fail(`Unexpected error in executeScript: ${e} :: ${e.stack}`);
    }
  });
});

add_task(async function noActiveTabForCurrentTab() {
  await openTwoTabsAndOpenTabMenu(async function onTabMenuClicked(info, tab) {
    const PAGE_URL = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";
    browser.test.assertEq(0, tab.index, "Expected a menu on the first tab.");
    browser.test.assertEq(PAGE_URL, tab.url, "Expected tab.url to be available for the first tab");

    let [tab2] = await browser.tabs.query({windowId: tab.windowId, index: 1});
    browser.test.assertTrue(tab2.active, "The second tab should be focused.");
    browser.test.assertEq(undefined, tab2.url, "Expected tab.url to be unavailable for the second tab.");

    await browser.test.assertRejects(
      browser.tabs.executeScript(tab2.id, {code: "document.URL"}),
      /Missing host permission for the tab/,
      "Content script should not run in the second tab");
    // (The activeTab permission was granted to the first tab, not tab2.)
  });
});
