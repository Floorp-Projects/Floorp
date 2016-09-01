var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);

add_task(function* () {
  registerCleanupFunction(function () {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    Services.prefs.clearUserPref("plugins.click_to_play");
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    gBrowser.removeCurrentTab();
    window.focus();
  });
});

add_task(function* () {
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  gBrowser.selectedTab = gBrowser.addTab();

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Second Test Plug-in");

  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_two_types.html");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gBrowser.selectedBrowser);

  // Test that the click-to-play doorhanger for multiple plugins shows the correct
  // state when re-opening without reloads or navigation.

  let pluginInfo = yield promiseForPluginInfo("test", gBrowser.selectedBrowser);
  ok(!pluginInfo.activated, "plugin should be activated");

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser);
  ok(notification, "Test 1a, Should have a click-to-play notification");

  yield promiseForNotificationShown(notification);

  is(notification.options.pluginData.size, 2,
      "Test 1a, Should have two types of plugin in the notification");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gBrowser.selectedBrowser);

  is(PopupNotifications.panel.firstChild.childNodes.length, 2, "have child nodes");

  let pluginItem = null;
  for (let item of PopupNotifications.panel.firstChild.childNodes) {
    is(item.value, "block", "Test 1a, all plugins should start out blocked");
    if (item.action.pluginName == "Test") {
      pluginItem = item;
    }
  }

  // Choose "Allow now" for the test plugin
  pluginItem.value = "allownow";
  PopupNotifications.panel.firstChild._primaryButton.click();

  pluginInfo = yield promiseForPluginInfo("test", gBrowser.selectedBrowser);
  ok(pluginInfo.activated, "plugin should be activated");

  notification = PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser);
  ok(notification, "Test 1b, Should have a click-to-play notification");

  yield promiseForNotificationShown(notification);

  pluginItem = null;
  for (let item of PopupNotifications.panel.firstChild.childNodes) {
    if (item.action.pluginName == "Test") {
      is(item.value, "allownow", "Test 1b, Test plugin should now be set to 'Allow now'");
    } else {
      is(item.value, "block", "Test 1b, Second Test plugin should still be blocked");
      pluginItem = item;
    }
  }

  // Choose "Allow and remember" for the Second Test plugin
  pluginItem.value = "allowalways";
  PopupNotifications.panel.firstChild._primaryButton.click();

  pluginInfo = yield promiseForPluginInfo("secondtestA", gBrowser.selectedBrowser);
  ok(pluginInfo.activated, "plugin should be activated");

  notification = PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser);
  ok(notification, "Test 1c, Should have a click-to-play notification");

  yield promiseForNotificationShown(notification);

  for (let item of PopupNotifications.panel.firstChild.childNodes) {
    if (item.action.pluginName == "Test") {
      is(item.value, "allownow", "Test 1c, Test plugin should be set to 'Allow now'");
    } else {
      is(item.value, "allowalways", "Test 1c, Second Test plugin should be set to 'Allow always'");
    }
  }
});
