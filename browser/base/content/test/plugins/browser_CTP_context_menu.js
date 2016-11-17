var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

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
});

// Test that the activate action in content menus for CTP plugins works
add_task(function* () {
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  gBrowser.selectedTab = gBrowser.addTab();

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  let bindingPromise = waitForEvent(gBrowser.selectedBrowser, "PluginBindingAttached", null, true, true);
  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");
  yield promiseUpdatePluginBindings(gBrowser.selectedBrowser);
  yield bindingPromise;

  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser);
  ok(popupNotification, "Test 1, Should have a click-to-play notification");

  // check plugin state
  let pluginInfo = yield promiseForPluginInfo("test", gBrowser.selectedBrowser);
  ok(!pluginInfo.activated, "plugin should not be activated");

  // Display a context menu on the test plugin so we can test
  // activation menu options.
  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    let plugin = content.document.getElementById("test");
    let bounds = plugin.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    utils.sendMouseEvent("contextmenu", left, top, 2, 1, 0);
  });

  popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser);
  ok(popupNotification, "Should have a click-to-play notification");
  ok(popupNotification.dismissed, "notification should be dismissed");

  // fixes a occasional test timeout on win7 opt
  yield promiseForCondition(() => document.getElementById("context-ctp-play"));

  let actMenuItem = document.getElementById("context-ctp-play");
  ok(actMenuItem, "Should have a context menu entry for activating the plugin");

  // Activate the plugin via the context menu
  EventUtils.synthesizeMouseAtCenter(actMenuItem, {});

  yield promiseForCondition(() => !PopupNotifications.panel.dismissed && PopupNotifications.panel.firstChild);

  // Activate the plugin
  PopupNotifications.panel.firstChild._primaryButton.click();

  // check plugin state
  pluginInfo = yield promiseForPluginInfo("test", gBrowser.selectedBrowser);
  ok(pluginInfo.activated, "plugin should not be activated");
});
