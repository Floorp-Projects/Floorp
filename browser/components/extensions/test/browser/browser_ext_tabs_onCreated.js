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
      browser.tabs.onCreated.addListener(tab => {
        browser.tabs.remove(tab.id);
        browser.test.sendMessage("onCreated", tab);
      });
      browser.tabs.onUpdated.addListener((tabId, changes) => {
        browser.test.assertEq(
          '["status"]',
          JSON.stringify(Object.keys(changes)),
          "Should get no update other than 'status' during tab creation."
        );
      });
      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");
  BrowserCommands.openTab();

  let tab = await extension.awaitMessage("onCreated");
  is(true, tab.active, "Tab should be active");

  await extension.unload();
});
