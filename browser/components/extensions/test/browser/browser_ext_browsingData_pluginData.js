/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "pluginHost",
  "@mozilla.org/plugin/host;1",
  "nsIPluginHost"
);

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://127.0.0.1:8888/"
);
const TEST_URL = TEST_ROOT + "file_clearplugindata.html";
const REFERENCE_DATE = Date.now();

add_task(async function testPluginData() {
  function background() {
    browser.test.onMessage.addListener(async (msg, options) => {
      // Plugins are disabled.  We are simply testing that these async APIs
      // complete without exceptions.
      if (msg == "removePluginData") {
        await browser.browsingData.removePluginData(options);
      } else {
        await browser.browsingData.remove(options, { pluginData: true });
      }
      browser.test.sendMessage("pluginDataRemoved");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData"],
    },
  });

  async function testRemovalMethod(method) {
    // NB: Since pplugins are disabled, there is never any data to clear.
    // We are really testing that these operations are no-ops.

    // Clear plugin data with no since value.

    // Load page to set data for the plugin.
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

    extension.sendMessage(method, {});
    await extension.awaitMessage("pluginDataRemoved");

    BrowserTestUtils.removeTab(tab);

    // Clear pluginData with recent since value.

    // Load page to set data for the plugin.
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

    extension.sendMessage(method, { since: REFERENCE_DATE - 20000 });
    await extension.awaitMessage("pluginDataRemoved");

    BrowserTestUtils.removeTab(tab);

    // Clear pluginData with old since value.

    // Load page to set data for the plugin.
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

    extension.sendMessage(method, { since: REFERENCE_DATE - 1000000 });
    await extension.awaitMessage("pluginDataRemoved");

    BrowserTestUtils.removeTab(tab);

    // Clear pluginData for specific hosts.

    // Load page to set data for the plugin.
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

    extension.sendMessage(method, { hostnames: ["bar.com", "baz.com"] });
    await extension.awaitMessage("pluginDataRemoved");

    BrowserTestUtils.removeTab(tab);

    // Clear pluginData for no hosts.

    // Load page to set data for the plugin.
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

    extension.sendMessage(method, { hostnames: [] });
    await extension.awaitMessage("pluginDataRemoved");

    BrowserTestUtils.removeTab(tab);
  }

  waitForExplicitFinish();
  await extension.startup();

  await testRemovalMethod("removePluginData");
  await testRemovalMethod("remove");

  await extension.unload();
  ok(true, "should get to the end without throwing an exception");
  finish();
});
