let gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
let gNewWindow = null;

add_task(function* () {
  registerCleanupFunction(function () {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    Services.prefs.clearUserPref("plugins.click_to_play");
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    gNewWindow.close();
    gNewWindow = null;
    window.focus();
  });
});

add_task(function* () {
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  gBrowser.selectedTab = gBrowser.addTab();

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gBrowser.selectedBrowser);

  yield promisePopupNotification("click-to-play-plugins");
});

add_task(function* () {
  gNewWindow = gBrowser.replaceTabWithWindow(gBrowser.selectedTab);

  // XXX technically can't load fire before we get this call???
  yield waitForEvent(gNewWindow, "load", null, true);

  yield promisePopupNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser);

  ok(PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser), "Should have a click-to-play notification in the tab in the new window");
  ok(!PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should not have a click-to-play notification in the old window now");
});

add_task(function* () {
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.swapBrowsersAndCloseOther(gBrowser.selectedTab, gNewWindow.gBrowser.selectedTab);

  yield promisePopupNotification("click-to-play-plugins", gBrowser.selectedBrowser);

  ok(PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should have a click-to-play notification in the initial tab again");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gBrowser.selectedBrowser);
});

add_task(function* () {
  yield promisePopupNotification("click-to-play-plugins");

  gNewWindow = gBrowser.replaceTabWithWindow(gBrowser.selectedTab);

  yield promiseWaitForFocus(gNewWindow);

  yield promisePopupNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser);
});

add_task(function* () {
  ok(PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser), "Should have a click-to-play notification in the tab in the new window");
  ok(!PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should not have a click-to-play notification in the old window now");

  let pluginInfo = yield promiseForPluginInfo("test", gNewWindow.gBrowser.selectedBrowser);
  ok(!pluginInfo.activated, "plugin should not be activated");

  yield ContentTask.spawn(gNewWindow.gBrowser.selectedBrowser, {}, function* () {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let bounds = plugin.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
  });

  let condition = function() !PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser).dismissed && gNewWindow.PopupNotifications.panel.firstChild;
  yield promiseForCondition(condition);
});

add_task(function* () {
  // Click the activate button on doorhanger to make sure it works
  gNewWindow.PopupNotifications.panel.firstChild._primaryButton.click();

  let pluginInfo = yield promiseForPluginInfo("test", gNewWindow.gBrowser.selectedBrowser);
  ok(pluginInfo.activated, "plugin should be activated");
});
