/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function move_discarded_to_window() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["tabs"] },
    background: async function () {
      // Create a discarded tab
      let url = "https://example.com/";
      let tab = await browser.tabs.create({ url, discarded: true });
      browser.test.assertEq(true, tab.discarded, "Tab should be discarded");
      browser.test.assertEq(url, tab.url, "Tab URL should be correct");

      // Create a new window
      let { id: windowId } = await browser.windows.create();

      // Move the tab into that window
      [tab] = await browser.tabs.move(tab.id, { windowId, index: -1 });
      browser.test.assertTrue(tab.discarded, "Tab should still be discarded");
      browser.test.assertEq(url, tab.url, "Tab URL should still be correct");

      await browser.windows.remove(windowId);
      browser.test.notifyPass("tabs.move");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.move");
  await extension.unload();
});

add_task(async function move_hidden_discarded_to_window() {
  let extensionWithoutTabsPermission = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["https://example.com/"],
    },
    background() {
      browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
        if (changeInfo.hidden) {
          browser.test.assertEq(
            tab.url,
            "https://example.com/?hideme",
            "tab.url is correctly observed without tabs permission"
          );
          browser.test.sendMessage("onUpdated_checked");
        }
      });
      // Listener with "urls" filter, regression test for
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1695346
      browser.tabs.onUpdated.addListener(
        (tabId, changeInfo, tab) => {
          browser.test.assertTrue(changeInfo.hidden, "tab was hidden");
          browser.test.sendMessage("onUpdated_urls_filter");
        },
        {
          properties: ["hidden"],
          urls: ["https://example.com/?hideme"],
        }
      );
    },
  });
  await extensionWithoutTabsPermission.startup();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["tabs", "tabHide"] },
    // ExtensionControlledPopup's populateDescription method requires an addon:
    useAddonManager: "temporary",
    async background() {
      let url = "https://example.com/?hideme";
      let tab = await browser.tabs.create({ url, discarded: true });
      await browser.tabs.hide(tab.id);

      let { id: windowId } = await browser.windows.create();

      // Move the tab into that window
      [tab] = await browser.tabs.move(tab.id, { windowId, index: -1 });
      browser.test.assertTrue(tab.discarded, "Tab should still be discarded");
      browser.test.assertTrue(tab.hidden, "Tab should still be hidden");
      browser.test.assertEq(url, tab.url, "Tab URL should still be correct");

      await browser.windows.remove(windowId);
      browser.test.notifyPass("move_hidden_discarded_to_window");
    },
  });

  await extension.startup();
  await extension.awaitFinish("move_hidden_discarded_to_window");
  await extension.unload();

  await extensionWithoutTabsPermission.awaitMessage("onUpdated_checked");
  await extensionWithoutTabsPermission.awaitMessage("onUpdated_urls_filter");
  await extensionWithoutTabsPermission.unload();
});
