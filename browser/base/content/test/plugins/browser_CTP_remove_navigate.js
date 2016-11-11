const gTestRoot = getRootDirectory(gTestPath);
const gHttpTestRoot = gTestRoot.replace("chrome://mochitests/content/",
                                        "http://127.0.0.1:8888/");

add_task(function* () {
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    Services.prefs.clearUserPref("plugins.click_to_play");
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    gBrowser.removeCurrentTab();
    window.focus();
  });

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Second Test Plug-in");
});

/**
 * Tests that if a plugin is removed just as we transition to
 * a different page, that we don't show the hidden plugin
 * notification bar on the new page.
 */
add_task(function* () {
  gBrowser.selectedTab = gBrowser.addTab();

  // Load up a page with a plugin...
  let notificationPromise = waitForNotificationBar("plugin-hidden", gBrowser.selectedBrowser);
  yield promiseTabLoadEvent(gBrowser.selectedTab, gHttpTestRoot + "plugin_small.html");
  yield promiseUpdatePluginBindings(gBrowser.selectedBrowser);
  yield notificationPromise;

  // Trigger the PluginRemoved event to be fired, and then immediately
  // browse to a new page.
  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    let plugin = content.document.getElementById("test");
    plugin.remove();
  });

  yield promiseTabLoadEvent(gBrowser.selectedTab, "about:mozilla");

  // There should be no hidden plugin notification bar at about:mozilla.
  let notificationBox = gBrowser.getNotificationBox(gBrowser.selectedBrowser);
  is(notificationBox.getNotificationWithValue("plugin-hidden"), null,
     "Expected no notification box");
});

/**
 * Tests that if a plugin is removed just as we transition to
 * a different page with a plugin, that we show the right notification
 * for the new page.
 */
add_task(function* () {
  // Load up a page with a plugin...
  let notificationPromise = waitForNotificationBar("plugin-hidden", gBrowser.selectedBrowser);
  yield promiseTabLoadEvent(gBrowser.selectedTab, gHttpTestRoot + "plugin_small.html");
  yield promiseUpdatePluginBindings(gBrowser.selectedBrowser);
  yield notificationPromise;

  // Trigger the PluginRemoved event to be fired, and then immediately
  // browse to a new page.
  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    let plugin = content.document.getElementById("test");
    plugin.remove();
  });
});

add_task(function* () {
  yield promiseTabLoadEvent(gBrowser.selectedTab, gHttpTestRoot + "plugin_small_2.html");
  let notification = yield waitForNotificationBar("plugin-hidden", gBrowser.selectedBrowser);
  ok(notification, "There should be a notification shown for the new page.");
  // Ensure that the notification is showing information about
  // the x-second-test plugin.
  let label = notification.label;
  ok(label.includes("Second Test"), "Should mention the second plugin");
});
