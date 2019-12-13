/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function test_onCreated_active() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    async background() {
      let created = false;
      let tabIds = (await browser.tabs.query({})).map(t => t.id);
      browser.tabs.onCreated.addListener(tab => {
        created = tab.id;
        browser.tabs.remove(tab.id);
        browser.test.sendMessage("onCreated", tab);
      });
      // We always get at least one onUpdated event when creating tabs.
      browser.tabs.onUpdated.addListener((tabId, changes, tab) => {
        // ignore tabs created prior to extension startup
        if (tabIds.includes(tabId)) {
          return;
        }
        browser.test.assertTrue(created, tabId, "tab created before updated");
        browser.test.notifyPass("onUpdated");
      });
      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");
  BrowserOpenTab();

  let tab = await extension.awaitMessage("onCreated");
  is(true, tab.active, "Tab should be active");
  await extension.awaitFinish("onUpdated");

  await extension.unload();
});
