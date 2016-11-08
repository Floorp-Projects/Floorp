var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;
var gNumPluginBindingsAttached = 0;

add_task(function* () {
  registerCleanupFunction(function () {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("plugins.click_to_play");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });
});

add_task(function* () {
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Second Test Plug-in");

  gTestBrowser.addEventListener("PluginBindingAttached", function () { gNumPluginBindingsAttached++ }, true, true);

  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_bug820497.html");

  yield promiseForCondition(function () { return gNumPluginBindingsAttached == 1; });

  yield ContentTask.spawn(gTestBrowser, null, () => {
    // Note we add the second plugin in the code farther down, so there's
    // no way we got here with anything but one plugin loaded.
    let doc = content.document;
    let testplugin = doc.getElementById("test");
    ok(testplugin, "should have test plugin");
    let secondtestplugin = doc.getElementById("secondtest");
    ok(!secondtestplugin, "should not yet have second test plugin");
  });

  yield promisePopupNotification("click-to-play-plugins");
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "should have a click-to-play notification");

  yield promiseForNotificationShown(notification);

  is(notification.options.pluginData.size, 1, "should be 1 type of plugin in the popup notification");

  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    XPCNativeWrapper.unwrap(content).addSecondPlugin();
  });

  yield promiseForCondition(function () { return gNumPluginBindingsAttached == 2; });

  yield ContentTask.spawn(gTestBrowser, null, () => {
    let doc = content.document;
    let testplugin = doc.getElementById("test");
    ok(testplugin, "should have test plugin");
    let secondtestplugin = doc.getElementById("secondtest");
    ok(secondtestplugin, "should have second test plugin");
  });

  notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);

  ok(notification, "should have popup notification");

  yield promiseForNotificationShown(notification);

  is(notification.options.pluginData.size, 2, "aited too long for 2 types of plugins in popup notification");
});
