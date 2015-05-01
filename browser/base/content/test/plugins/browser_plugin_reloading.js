let gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
let gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);
let gTestBrowser = null;

function updateAllTestPlugins(aState) {
  setTestPluginEnabledState(aState, "Test Plug-in");
  setTestPluginEnabledState(aState, "Second Test Plug-in");
}

add_task(function* () {
  registerCleanupFunction(Task.async(function*() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("plugins.click_to_play");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    yield asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);
    resetBlocklist();
    gTestBrowser = null;
    gBrowser.removeCurrentTab();
    window.focus();
  }));
});

add_task(function* () {
  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;

  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  // Prime the blocklist service, the remote service doesn't launch on startup.
  yield promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html></html>");
  let exmsg = yield promiseInitContentBlocklistSvc(gBrowser.selectedBrowser);
  ok(!exmsg, "exception: " + exmsg);

  yield asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);
});

// Tests that a click-to-play plugin retains its activated state upon reloading
add_task(function* () {
  clearAllPluginPermissions();

  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 1, Should have a click-to-play notification");

  let pluginInfo = yield promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
     "Test 2, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");

  // run the plugin
  yield promisePlayObject("test");

  yield promiseUpdatePluginBindings(gTestBrowser);

  pluginInfo = yield promiseForPluginInfo("test");
  is(pluginInfo.displayedType, Ci.nsIObjectLoadingContent.TYPE_PLUGIN, "Test 3, plugin should have started");
  ok(pluginInfo.activated, "Test 4, plugin node should not be activated");

  let result = yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let plugin = content.document.getElementById("test");
    let npobj1 = Components.utils.waiveXrays(plugin).getObjectValue();
    plugin.src = plugin.src;
    let pluginsDiffer = false;
     try {
       Components.utils.waiveXrays(plugin).checkObjectValue(npobj1);
     } catch (e) {
       pluginsDiffer = true;
     }
     return pluginsDiffer;
  });
  ok(result, "Test 5, plugins differ.");

  pluginInfo = yield promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 6, Plugin should have retained activated state.");
  is(pluginInfo.displayedType, Ci.nsIObjectLoadingContent.TYPE_PLUGIN, "Test 7, plugin should have started");
});
