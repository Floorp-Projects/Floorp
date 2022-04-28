/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_middle_click_with_activeTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      page_action: {},
      permissions: ["activeTab"],
    },

    async background() {
      browser.pageAction.onClicked.addListener(async (tab, info) => {
        browser.test.assertEq(1, info.button, "Expected button value");
        browser.test.assertEq(
          "https://example.com/",
          tab.url,
          "tab.url has the expected url"
        );
        await browser.tabs.insertCSS(tab.id, {
          code: "body { border: 20px solid red; }",
        });
        browser.test.sendMessage("onClick");
      });

      let [tab] = await browser.tabs.query({
        active: true,
        currentWindow: true,
      });
      await browser.pageAction.show(tab.id);
      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  let ext = WebExtensionPolicy.getByID(extension.id).extension;
  is(
    ext.tabManager.hasActiveTabPermission(tab),
    false,
    "Active tab was not granted permission"
  );

  await clickPageAction(extension, window, { button: 1 });
  await extension.awaitMessage("onClick");

  is(
    ext.tabManager.hasActiveTabPermission(tab),
    true,
    "Active tab was granted permission"
  );
  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_middle_click_without_activeTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      page_action: {},
    },

    async background() {
      browser.pageAction.onClicked.addListener(async (tab, info) => {
        browser.test.assertEq(1, info.button, "Expected button value");
        browser.test.assertEq(tab.url, undefined, "tab.url is undefined");
        await browser.test.assertRejects(
          browser.tabs.insertCSS(tab.id, {
            code: "body { border: 20px solid red; }",
          }),
          "Missing host permission for the tab",
          "expected failure of tabs.insertCSS without permission"
        );
        browser.test.sendMessage("onClick");
      });

      let [tab] = await browser.tabs.query({
        active: true,
        currentWindow: true,
      });
      await browser.pageAction.show(tab.id);
      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  await clickPageAction(extension, window, { button: 1 });
  await extension.awaitMessage("onClick");

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
