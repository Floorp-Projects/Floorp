/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function tabsAttention() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/?2",
    true
  );
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/?1",
    true
  );
  gBrowser.selectedTab = tab2;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs", "http://example.com/*"],
    },

    background: async function() {
      function onActive(tabId, changeInfo, tab) {
        browser.test.assertFalse(
          changeInfo.attention,
          "changeInfo.attention should be false"
        );
        browser.test.assertFalse(
          tab.attention,
          "tab.attention should be false"
        );
        browser.test.assertTrue(tab.active, "tab.active should be true");
        browser.test.notifyPass("tabsAttention");
      }

      function onUpdated(tabId, changeInfo, tab) {
        browser.test.assertTrue(
          changeInfo.attention,
          "changeInfo.attention should be true"
        );
        browser.test.assertTrue(tab.attention, "tab.attention should be true");
        browser.tabs.onUpdated.removeListener(onUpdated);
        browser.tabs.onUpdated.addListener(onActive);
        browser.tabs.update(tabId, { active: true });
      }

      browser.tabs.onUpdated.addListener(onUpdated, {
        properties: ["attention"],
      });
      const tabs = await browser.tabs.query({ index: 1 });
      browser.tabs.executeScript(tabs[0].id, {
        code: `alert("tab attention")`,
      });
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabsAttention");
  await extension.unload();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
