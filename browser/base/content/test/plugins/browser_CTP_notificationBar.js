var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;

add_task(async function() {
  await SpecialPowers.pushPrefEnv({ set: [
    ["plugins.show_infobar", true],
    ["plugins.click_to_play", true],
    ["extensions.blocklist.supressUI", true],
  ]});

  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });

  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
});

add_task(async function() {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_small.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await promisePopupNotification("click-to-play-plugins");

  // Expecting a notification bar for hidden plugins
  await promiseForNotificationBar("plugin-hidden", gTestBrowser);
});

add_task(async function() {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_small.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  await promiseForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") === null);
});

add_task(async function() {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_overlayed.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  // Expecting a plugin notification bar when plugins are overlaid.
  await promiseForNotificationBar("plugin-hidden", gTestBrowser);
});

add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_overlayed.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.equal(plugin.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
      "Test 3b, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");
  });

  let pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 1a, plugin should not be activated");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(!(overlay && overlay.classList.contains("visible")),
      "Test 3b, overlay should be hidden.");
  });
});

add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_positioned.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  // Expecting a plugin notification bar when plugins are overlaid offscreen.
  await promisePopupNotification("click-to-play-plugins");
  await promiseForNotificationBar("plugin-hidden", gTestBrowser);

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.equal(plugin.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
      "Test 4b, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");
  });

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(!(overlay && overlay.classList.contains("visible")),
      "Test 4b, overlay should be hidden.");
  });
});

// Test that the notification bar is getting dismissed when directly activating plugins
// via the doorhanger.

add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_small.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  // Expecting a plugin notification bar when plugins are overlaid offscreen.
  await promisePopupNotification("click-to-play-plugins");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.equal(plugin.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
      "Test 6, Plugin should be click-to-play");
  });

  await promisePopupNotification("click-to-play-plugins");

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 6, Should have a click-to-play notification");

  // simulate "always allow"
  await promiseForNotificationShown(notification);

  PopupNotifications.panel.firstChild._primaryButton.click();

  let notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  await promiseForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") === null);

  let pluginInfo = await promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 7, plugin should be activated");
});
