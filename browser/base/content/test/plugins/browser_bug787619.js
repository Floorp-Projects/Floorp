var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;
var gWrapperClickCount = 0;

add_task(function* () {
  registerCleanupFunction(function() {
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

  let testRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
  yield promiseTabLoadEvent(gBrowser.selectedTab, testRoot + "plugin_bug787619.html");

  // Due to layout being async, "PluginBindAttached" may trigger later.
  // This forces a layout flush, thus triggering it, and schedules the
  // test so it is definitely executed afterwards.
  yield promiseUpdatePluginBindings(gTestBrowser);

  // check plugin state
  let pluginInfo = yield promiseForPluginInfo("plugin");
  ok(!pluginInfo.activated, "1a plugin should not be activated");

  // click the overlay to prompt
  let promise = promisePopupNotification("click-to-play-plugins");
  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let plugin = content.document.getElementById("plugin");
    let bounds = plugin.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
  });
  yield promise;

  // check plugin state
  pluginInfo = yield promiseForPluginInfo("plugin");
  ok(!pluginInfo.activated, "1b plugin should not be activated");

  let condition = () => !PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed &&
    PopupNotifications.panel.firstChild;
  yield promiseForCondition(condition);
  PopupNotifications.panel.firstChild._primaryButton.click();

  // check plugin state
  pluginInfo = yield promiseForPluginInfo("plugin");
  ok(pluginInfo.activated, "plugin should be activated");

  is(gWrapperClickCount, 0, 'wrapper should not have received any clicks');
});
