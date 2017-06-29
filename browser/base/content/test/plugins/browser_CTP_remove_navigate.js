const gTestRoot = getRootDirectory(gTestPath);
const gHttpTestRoot = gTestRoot.replace("chrome://mochitests/content/",
                                        "http://127.0.0.1:8888/");

add_task(async function() {
  await SpecialPowers.pushPrefEnv({ set: [
    ["plugins.click_to_play", true],
    ["extensions.blocklist.suppressUI", true],
    ["plugins.show_infobar", true],
  ]});
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    gBrowser.removeCurrentTab();
    window.focus();
  });

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Second Test Plug-in");
});

/**
 * Tests that if a plugin is removed just as we transition to
 * a different page, that we don't show the hidden plugin
 * notification bar on the new page.
 */
add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  // Load up a page with a plugin...
  let notificationPromise = waitForNotificationBar("plugin-hidden", gBrowser.selectedBrowser);
  await promiseTabLoadEvent(gBrowser.selectedTab, gHttpTestRoot + "plugin_small.html");
  await promiseUpdatePluginBindings(gBrowser.selectedBrowser);
  await notificationPromise;

  // Trigger the PluginRemoved event to be fired, and then immediately
  // browse to a new page.
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    let plugin = content.document.getElementById("test");
    plugin.remove();
  });

  await promiseTabLoadEvent(gBrowser.selectedTab, "about:mozilla");

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
add_task(async function() {
  // Load up a page with a plugin...
  let notificationPromise = waitForNotificationBar("plugin-hidden", gBrowser.selectedBrowser);
  await promiseTabLoadEvent(gBrowser.selectedTab, gHttpTestRoot + "plugin_small.html");
  await promiseUpdatePluginBindings(gBrowser.selectedBrowser);
  await notificationPromise;

  // Trigger the PluginRemoved event to be fired, and then immediately
  // browse to a new page.
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    let plugin = content.document.getElementById("test");
    plugin.remove();
  });
});

add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gHttpTestRoot + "plugin_small_2.html");
  let notification = await waitForNotificationBar("plugin-hidden", gBrowser.selectedBrowser);
  ok(notification, "There should be a notification shown for the new page.");
});
