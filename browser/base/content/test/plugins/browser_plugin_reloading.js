var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gPluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
var gTestBrowser = null;

function updateAllTestPlugins(aState) {
  setTestPluginEnabledState(aState, "Test Plug-in");
  setTestPluginEnabledState(aState, "Second Test Plug-in");
}

add_task(async function() {
  registerCleanupFunction(async function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("plugins.click_to_play");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins", gTestBrowser);
    resetBlocklist();
    gTestBrowser = null;
    gBrowser.removeCurrentTab();
    window.focus();
  });
});

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gTestBrowser = gBrowser.selectedBrowser;

  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  // Prime the blocklist service, the remote service doesn't launch on startup.
  await promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html></html>");

  await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins", gTestBrowser);
});

// Tests that a click-to-play plugin retains its activated state upon reloading
add_task(async function() {
  clearAllPluginPermissions();

  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 1, Should have a click-to-play notification");

  let pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
     "Test 2, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");

  // run the plugin
  await promisePlayObject("test");

  await promiseUpdatePluginBindings(gTestBrowser);

  pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.displayedType, Ci.nsIObjectLoadingContent.TYPE_PLUGIN, "Test 3, plugin should have started");
  ok(pluginInfo.activated, "Test 4, plugin node should not be activated");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let plugin = content.document.getElementById("test");
    let npobj1 = Cu.waiveXrays(plugin).getObjectValue();
    // eslint-disable-next-line no-self-assign
    plugin.src = plugin.src;
    let pluginsDiffer = false;
    try {
      Cu.waiveXrays(plugin).checkObjectValue(npobj1);
    } catch (e) {
      pluginsDiffer = true;
    }

     Assert.ok(pluginsDiffer, "Test 5, plugins differ.");
  });

  pluginInfo = await promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 6, Plugin should have retained activated state.");
  is(pluginInfo.displayedType, Ci.nsIObjectLoadingContent.TYPE_PLUGIN, "Test 7, plugin should have started");
});
