var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gNewWindow = null;

add_task(async function() {
  registerCleanupFunction(function() {
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

add_task(async function() {
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gBrowser.selectedBrowser);

  await promisePopupNotification("click-to-play-plugins");
});

add_task(async function() {
  gNewWindow = gBrowser.replaceTabWithWindow(gBrowser.selectedTab);

  // XXX technically can't load fire before we get this call???
  await waitForEvent(gNewWindow, "load", null, true);

  await promisePopupNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser);

  ok(PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser), "Should have a click-to-play notification in the tab in the new window");
  ok(!PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should not have a click-to-play notification in the old window now");
});

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.swapBrowsersAndCloseOther(gBrowser.selectedTab, gNewWindow.gBrowser.selectedTab);

  await promisePopupNotification("click-to-play-plugins", gBrowser.selectedBrowser);

  ok(PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should have a click-to-play notification in the initial tab again");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gBrowser.selectedBrowser);
});

add_task(async function() {
  await promisePopupNotification("click-to-play-plugins");

  gNewWindow = gBrowser.replaceTabWithWindow(gBrowser.selectedTab);

  await promiseWaitForFocus(gNewWindow);

  await promisePopupNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser);
});

add_task(async function() {
  ok(PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser), "Should have a click-to-play notification in the tab in the new window");
  ok(!PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser), "Should not have a click-to-play notification in the old window now");

  let pluginInfo = await promiseForPluginInfo("test", gNewWindow.gBrowser.selectedBrowser);
  ok(!pluginInfo.activated, "plugin should not be activated");

  await ContentTask.spawn(gNewWindow.gBrowser.selectedBrowser, {}, async function() {
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

  let condition = () => !PopupNotifications.getNotification("click-to-play-plugins", gNewWindow.gBrowser.selectedBrowser).dismissed && gNewWindow.PopupNotifications.panel.firstChild;
  await promiseForCondition(condition);
});

add_task(async function() {
  // Click the activate button on doorhanger to make sure it works
  gNewWindow.PopupNotifications.panel.firstChild.button.click();

  let pluginInfo = await promiseForPluginInfo("test", gNewWindow.gBrowser.selectedBrowser);
  ok(pluginInfo.activated, "plugin should be activated");
});
