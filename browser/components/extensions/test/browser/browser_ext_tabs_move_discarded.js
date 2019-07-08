/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["tabs"] },
    background: async function() {
      // Create a discarded tab
      let url = "http://example.com/";
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
