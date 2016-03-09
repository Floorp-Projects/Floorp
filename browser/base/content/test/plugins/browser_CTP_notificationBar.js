var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;

add_task(function* () {
  registerCleanupFunction(function () {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    Services.prefs.clearUserPref("plugins.click_to_play");
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });

  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
});

add_task(function* () {
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_small.html");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  yield promisePopupNotification("click-to-play-plugins");

  // Expecting a notification bar for hidden plugins
  yield promiseForNotificationBar("plugin-hidden", gTestBrowser);
});

add_task(function* () {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");

  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_small.html");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  let notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  yield promiseForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") === null);
});

add_task(function* () {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_overlayed.html");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  // Expecting a plugin notification bar when plugins are overlaid.
  yield promiseForNotificationBar("plugin-hidden", gTestBrowser);
});

add_task(function* () {
  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_overlayed.html");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  yield ContentTask.spawn(gTestBrowser, null, function* () {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.equal(plugin.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
      "Test 3b, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");
  });

  let pluginInfo = yield promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 1a, plugin should not be activated");

  yield ContentTask.spawn(gTestBrowser, null, function* () {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(!(overlay && overlay.classList.contains("visible")),
      "Test 3b, overlay should be hidden.");
  });
});

add_task(function* () {
  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_positioned.html");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  // Expecting a plugin notification bar when plugins are overlaid offscreen.
  yield promisePopupNotification("click-to-play-plugins");
  yield promiseForNotificationBar("plugin-hidden", gTestBrowser);

  yield ContentTask.spawn(gTestBrowser, null, function* () {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.equal(plugin.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
      "Test 4b, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");
  });

  yield ContentTask.spawn(gTestBrowser, null, function* () {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(!(overlay && overlay.classList.contains("visible")),
      "Test 4b, overlay should be hidden.");
  });
});

// Test that the notification bar is getting dismissed when directly activating plugins
// via the doorhanger.

add_task(function* () {
  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_small.html");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  // Expecting a plugin notification bar when plugins are overlaid offscreen.
  yield promisePopupNotification("click-to-play-plugins");

  yield ContentTask.spawn(gTestBrowser, null, function* () {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.equal(plugin.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
      "Test 6, Plugin should be click-to-play");
  });

  yield promisePopupNotification("click-to-play-plugins");

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 6, Should have a click-to-play notification");

  // simulate "always allow"
  yield promiseForNotificationShown(notification);

  PopupNotifications.panel.firstChild._primaryButton.click();

  let notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  yield promiseForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") === null);

  let pluginInfo = yield promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 7, plugin should be activated");
});
