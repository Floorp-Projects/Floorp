/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testWarmupTab() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.net/"
  );
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  Assert.ok(!tab1.linkedBrowser.renderLayers, "tab is not warm yet");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    background: async function () {
      let backgroundTab = (
        await browser.tabs.query({
          lastFocusedWindow: true,
          url: "http://example.net/",
          active: false,
        })
      )[0];
      await browser.tabs.warmup(backgroundTab.id);
      browser.test.notifyPass("tabs.warmup");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.warmup");
  Assert.ok(tab1.linkedBrowser.renderLayers, "tab has been warmed up");
  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
  await extension.unload();
});
