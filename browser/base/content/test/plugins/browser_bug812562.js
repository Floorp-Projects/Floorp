var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;

add_task(function* () {
  registerCleanupFunction(Task.async(function*() {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    yield asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);
    resetBlocklist();
    Services.prefs.clearUserPref("plugins.click_to_play");
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  // Prime the blocklist service, the remote service doesn't launch on startup.
  yield promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html></html>");
  let exmsg = yield promiseInitContentBlocklistSvc(gBrowser.selectedBrowser);
  ok(!exmsg, "exception: " + exmsg);
});

// Tests that the going back will reshow the notification for click-to-play
// blocklisted plugins
add_task(function* () {
  yield asyncSetAndUpdateBlocklist(gTestRoot + "blockPluginVulnerableUpdatable.xml", gTestBrowser);

  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "test part 1: Should have a click-to-play notification");

  let pluginInfo = yield promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE, "plugin should be marked as VULNERABLE");

  yield ContentTask.spawn(gTestBrowser, null, function* () {
    Assert.ok(!!content.document.getElementById("test"),
      "test part 1: plugin should not be activated");
  });

  yield promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html></html>");
});

add_task(function* () {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "test part 2: Should not have a click-to-play notification");
  yield ContentTask.spawn(gTestBrowser, null, function* () {
    Assert.ok(!content.document.getElementById("test"),
      "test part 2: plugin should not be activated");
  });

  let obsPromise = TestUtils.topicObserved("PopupNotifications-updateNotShowing");
  let overlayPromise = promisePopupNotification("click-to-play-plugins");
  gTestBrowser.goBack();
  yield obsPromise;
  yield overlayPromise;
});

add_task(function* () {
  yield promiseUpdatePluginBindings(gTestBrowser);

  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "test part 3: Should have a click-to-play notification");

  let pluginInfo = yield promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE, "plugin should be marked as VULNERABLE");

  yield ContentTask.spawn(gTestBrowser, null, function* () {
    Assert.ok(!!content.document.getElementById("test"),
      "test part 3: plugin should not be activated");
  });
});
