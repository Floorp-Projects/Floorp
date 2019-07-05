/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "pluginHost",
  "@mozilla.org/plugin/host;1",
  "nsIPluginHost"
);

// Returns the chrome side nsIPluginTag for the test plugin.
function getTestPlugin() {
  let tags = pluginHost.getPluginTags();
  let plugin = tags.find(tag => tag.name == "Test Plug-in");
  if (!plugin) {
    ok(false, "Unable to find plugin");
  }
  return plugin;
}

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://127.0.0.1:8888/"
);
const TEST_URL = TEST_ROOT + "file_clearplugindata.html";
const REFERENCE_DATE = Date.now();
const PLUGIN_TAG = getTestPlugin();

/* Due to layout being async, "PluginBindAttached" may trigger later. This
   returns a Promise that resolves once we've forced a layout flush, which
   triggers the PluginBindAttached event to fire. This trick only works if
   there is some sort of plugin in the page.
 */
function promiseUpdatePluginBindings(browser) {
  return ContentTask.spawn(browser, {}, async function() {
    let doc = content.document;
    let elems = doc.getElementsByTagName("embed");
    if (elems && elems.length > 0) {
      elems[0].clientTop; // eslint-disable-line no-unused-expressions
    }
  });
}

function stored(needles) {
  let something = pluginHost.siteHasData(PLUGIN_TAG, null);
  if (!needles) {
    return something;
  }

  if (!something) {
    return false;
  }

  if (needles.every(value => pluginHost.siteHasData(PLUGIN_TAG, value))) {
    return true;
  }

  return false;
}

add_task(async function testPluginData() {
  function background() {
    browser.test.onMessage.addListener(async (msg, options) => {
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
    // Clear plugin data with no since value.

    // Load page to set data for the plugin.
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
    await promiseUpdatePluginBindings(gBrowser.selectedBrowser);

    ok(
      stored(["foo.com", "bar.com", "baz.com", "qux.com"]),
      "Data stored for sites"
    );

    extension.sendMessage(method, {});
    await extension.awaitMessage("pluginDataRemoved");

    ok(!stored(null), "All data cleared");
    BrowserTestUtils.removeTab(tab);

    // Clear history with recent since value.

    // Load page to set data for the plugin.
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
    await promiseUpdatePluginBindings(gBrowser.selectedBrowser);

    ok(
      stored(["foo.com", "bar.com", "baz.com", "qux.com"]),
      "Data stored for sites"
    );

    extension.sendMessage(method, { since: REFERENCE_DATE - 20000 });
    await extension.awaitMessage("pluginDataRemoved");

    ok(stored(["bar.com", "qux.com"]), "Data stored for sites");
    ok(!stored(["foo.com"]), "Data cleared for foo.com");
    ok(!stored(["baz.com"]), "Data cleared for baz.com");
    BrowserTestUtils.removeTab(tab);

    // Clear history with old since value.

    // Load page to set data for the plugin.
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
    await promiseUpdatePluginBindings(gBrowser.selectedBrowser);

    ok(
      stored(["foo.com", "bar.com", "baz.com", "qux.com"]),
      "Data stored for sites"
    );

    extension.sendMessage(method, { since: REFERENCE_DATE - 1000000 });
    await extension.awaitMessage("pluginDataRemoved");

    ok(!stored(null), "All data cleared");
    BrowserTestUtils.removeTab(tab);
  }

  PLUGIN_TAG.enabledState = Ci.nsIPluginTag.STATE_ENABLED;

  await extension.startup();

  await testRemovalMethod("removePluginData");
  await testRemovalMethod("remove");

  await extension.unload();
});
