"use strict";

const TEST_PLUGIN_NAME = "Test Plug-in";
const HIDDEN_CTP_PLUGIN_PREF = "plugins.navigator.hidden_ctp_plugin";

/**
 * If a plugin is click-to-play and named in HIDDEN_CTP_PLUGIN_PREF,
 * then the plugin should be hidden in the navigator.plugins list by default
 * when iterating.
 */

add_task(async function setup() {
  // We'll make the Test Plugin click-to-play.
  let originalPluginState = getTestPluginEnabledState();
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);
  registerCleanupFunction(() => {
    setTestPluginEnabledState(originalPluginState);
  });

  // And then make the plugin hidden.
  await SpecialPowers.pushPrefEnv({
    set: [
      [HIDDEN_CTP_PLUGIN_PREF, TEST_PLUGIN_NAME],
      ["plugins.show_infobar", true],
    ],
  });
});

/**
 * Tests that if a plugin is click-to-play and in the
 * HIDDEN_CTP_PLUGIN_PREF list, then it shouldn't be visible
 * when iterating navigator.plugins.
 */
add_task(async function test_plugin_is_hidden_on_iteration() {
  // The plugin should not be visible when we iterate
  // navigator.plugins.
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, async function(browser) {
    await ContentTask.spawn(browser, TEST_PLUGIN_NAME, async function(pluginName) {
      let plugins = Array.from(content.navigator.plugins);
      Assert.ok(plugins.every(p => p.name != pluginName),
                "Should not find Test Plugin");
    });
  });

  // Now clear the HIDDEN_CTP_PLUGIN_PREF temporarily and
  // make sure we can see the plugin again.
  await SpecialPowers.pushPrefEnv({
    set: [[HIDDEN_CTP_PLUGIN_PREF, ""]],
  });

  // Note that I have to do this in a new tab since navigator
  // caches navigator.plugins after an initial read.
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, async function(browser) {
    await ContentTask.spawn(browser, TEST_PLUGIN_NAME, async function(pluginName) {
      let plugins = Array.from(content.navigator.plugins);
      Assert.ok(plugins.some(p => p.name == pluginName),
                "Should have found the Test Plugin");
    });
  });

  await SpecialPowers.popPrefEnv();
});
