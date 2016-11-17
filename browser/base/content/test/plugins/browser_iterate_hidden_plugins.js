"use strict";

const TEST_PLUGIN_NAME = "Test Plug-in";
const HIDDEN_CTP_PLUGIN_PREF = "plugins.navigator.hidden_ctp_plugin";

/**
 * If a plugin is click-to-play and named in HIDDEN_CTP_PLUGIN_PREF,
 * then the plugin should be hidden in the navigator.plugins list by default
 * when iterating. If, however, JS attempts to access the plugin via
 * navigator.plugins[NAME] directly, we should show the "Hidden Plugin"
 * notification bar.
 */

add_task(function* setup() {
  // We'll make the Test Plugin click-to-play.
  let originalPluginState = getTestPluginEnabledState();
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);
  registerCleanupFunction(() => {
    setTestPluginEnabledState(originalPluginState);
  });

  // And then make the plugin hidden.
  yield SpecialPowers.pushPrefEnv({
    set: [[HIDDEN_CTP_PLUGIN_PREF, TEST_PLUGIN_NAME]],
  })
});

/**
 * Tests that if a plugin is click-to-play and in the
 * HIDDEN_CTP_PLUGIN_PREF list, then it shouldn't be visible
 * when iterating navigator.plugins.
 */
add_task(function* test_plugin_is_hidden_on_iteration() {
  // The plugin should not be visible when we iterate
  // navigator.plugins.
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com"
  }, function*(browser) {
    yield ContentTask.spawn(browser, TEST_PLUGIN_NAME, function*(pluginName) {
      let plugins = Array.from(content.navigator.plugins);
      Assert.ok(plugins.every(p => p.name != pluginName),
                "Should not find Test Plugin");
    });
  });

  // Now clear the HIDDEN_CTP_PLUGIN_PREF temporarily and
  // make sure we can see the plugin again.
  yield SpecialPowers.pushPrefEnv({
    set: [[HIDDEN_CTP_PLUGIN_PREF, ""]],
  });

  // Note that I have to do this in a new tab since navigator
  // caches navigator.plugins after an initial read.
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com"
  }, function*(browser) {
    yield ContentTask.spawn(browser, TEST_PLUGIN_NAME, function*(pluginName) {
      let plugins = Array.from(content.navigator.plugins);
      Assert.ok(plugins.some(p => p.name == pluginName),
                "Should have found the Test Plugin");
    });
  });

  yield SpecialPowers.popPrefEnv();
});

/**
 * Tests that if a click-to-play plugin is hidden, that we show the
 * Hidden Plugin notification bar when accessed directly by name
 * via navigator.plugins.
 */
add_task(function* test_plugin_shows_hidden_notification_on_access() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com"
  }, function*(browser) {
    let notificationPromise = waitForNotificationBar("plugin-hidden", gBrowser.selectedBrowser);

    yield ContentTask.spawn(browser, TEST_PLUGIN_NAME, function*(pluginName) {
      let plugins = content.navigator.plugins;
      // Just accessing the plugin should be enough to trigger the notification.
      // We'll also do a sanity check and make sure that the HiddenPlugin event
      // is firing (which is the event that triggers the notification bar).
      let sawEvent = false;
      addEventListener("HiddenPlugin", function onHiddenPlugin(e) {
        sawEvent = true;
        removeEventListener("HiddenPlugin", onHiddenPlugin, true);
      }, true);
      plugins[pluginName];
      Assert.ok(sawEvent, "Should have seen the HiddenPlugin event.");
    });

    let notification = yield notificationPromise;
    notification.close();
  });

  // Make sure that if the plugin wasn't hidden that touching it
  // does _NOT_ show the notification bar.
  yield SpecialPowers.pushPrefEnv({
    set: [[HIDDEN_CTP_PLUGIN_PREF, ""]],
  });

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com"
  }, function*(browser) {
    yield ContentTask.spawn(browser, TEST_PLUGIN_NAME, function*(pluginName) {
      let plugins = content.navigator.plugins;
      // Instead of waiting for a notification bar that should never come,
      // we'll make sure that the HiddenPlugin event never fires in content
      // (which is the event that triggers the notification bar).
      let sawEvent = false;
      addEventListener("HiddenPlugin", function onHiddenPlugin(e) {
        sawEvent = true;
        removeEventListener("HiddenPlugin", onHiddenPlugin, true);
      }, true);
      plugins[pluginName];
      Assert.ok(!sawEvent, "Should not have seen the HiddenPlugin event.");
    });
  });

  yield SpecialPowers.popPrefEnv();
});
