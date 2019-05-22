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

// Tests a page with an unknown plugin in it.
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_unknown.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let pluginInfo = await promiseForPluginInfo("unknown");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_UNSUPPORTED,
     "Test 1a, plugin fallback type should be PLUGIN_UNSUPPORTED");
});

// Test that the doorhanger is shown when the user clicks on the overlay
// after having previously blocked the plugin.
add_task(async function() {
  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await promisePopupNotification("click-to-play-plugins");

  let pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Plugin should not be activated");

  // Simulate clicking the "Allow" button.
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);

  await promiseForNotificationShown(notification);

  PopupNotifications.panel.firstElementChild.button.click();

  pluginInfo = await promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Plugin should be activated");

  // Simulate clicking the "Block" button.
  await promiseForNotificationShown(notification);

  PopupNotifications.panel.firstElementChild.secondaryButton.click();

  pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Plugin should not be activated");

  let browserLoaded = BrowserTestUtils.browserLoaded(gTestBrowser);
  gTestBrowser.reload();
  await browserLoaded;
  notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);

  // Simulate clicking the overlay
  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let bounds = plugin.openOrClosedShadowRoot.getElementById("main").getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.windowUtils;
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
  });

  ok(!notification.dismissed, "A plugin notification should be shown.");

  clearAllPluginPermissions();
});

// Tests that going back will reshow the notification for click-to-play plugins
add_task(async function() {
  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await promisePopupNotification("click-to-play-plugins");

  await promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html>hi</html>");

  // make sure the notification is gone
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!notification, "Test 11b, Should not have a click-to-play notification");

  gTestBrowser.webNavigation.goBack();

  await promisePopupNotification("click-to-play-plugins");
});

// Tests that the "Allow Always" permission works for click-to-play plugins
add_task(async function() {
  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await promisePopupNotification("click-to-play-plugins");

  let pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 12a, Plugin should not be activated");

  // Simulate clicking the "Allow" button.
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);

  await promiseForNotificationShown(notification);

  PopupNotifications.panel.firstElementChild.button.click();

  pluginInfo = await promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 12a, Plugin should be activated");
});

// Test that the "Always" permission, when set for just the Test plugin,
// does not also allow the Second Test plugin.
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_two_types.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await promisePopupNotification("click-to-play-plugins");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let test = content.document.getElementById("test");
    let secondtestA = content.document.getElementById("secondtestA");
    let secondtestB = content.document.getElementById("secondtestB");
    Assert.ok(test.activated && !secondtestA.activated && !secondtestB.activated,
      "Content plugins are set up");
  });

  clearAllPluginPermissions();
});

// Tests that the plugin's "activated" property is true for working plugins
// with click-to-play disabled.
add_task(async function() {
  updateAllTestPlugins(Ci.nsIPluginTag.STATE_ENABLED);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test2.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let pluginInfo = await promiseForPluginInfo("test1");
  ok(pluginInfo.activated, "Test 14, Plugin should be activated");
});

// Tests that the overlay is shown instead of alternate content when
// plugins are click to play.
add_task(async function() {
  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_alternate_content.html");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let mainBox = plugin.openOrClosedShadowRoot.getElementById("main");
    Assert.ok(!!mainBox, "Test 15, Plugin overlay should exist");
  });
});

// Tests that mContentType is used for click-to-play plugins, and not the
// inspected type.
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_bug749455.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 17, Should have a click-to-play notification");
});

// Tests that clicking the icon of the overlay activates the doorhanger
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins", gTestBrowser);

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 18g, Plugin should not be activated");

  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed,
     "Test 19a, Doorhanger should start out dismissed");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let icon = plugin.openOrClosedShadowRoot.getElementById("icon");
    let bounds = icon.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.windowUtils;
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
  });

  let condition = () => !PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed;
  await promiseForCondition(condition);
});

// Tests that clicking the text of the overlay activates the plugin
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 18g, Plugin should not be activated");

  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed,
     "Test 19c, Doorhanger should start out dismissed");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let text = plugin.openOrClosedShadowRoot.getElementById("clickToPlay");
    let bounds = text.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.windowUtils;
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
  });

  let condition = () => !PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed;
  await promiseForCondition(condition);
});

// Tests that clicking the box of the overlay activates the doorhanger
// (just to be thorough)
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 18g, Plugin should not be activated");

  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed,
     "Test 19e, Doorhanger should start out dismissed");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let utils = content.windowUtils;
    utils.sendMouseEvent("mousedown", 50, 50, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", 50, 50, 0, 1, 0, false, 0, 0);
  });

  let condition = () => !PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed &&
    PopupNotifications.panel.firstElementChild;
  await promiseForCondition(condition);
  PopupNotifications.panel.firstElementChild.button.click();

  pluginInfo = await promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 19e, Plugin should not be activated");

  clearAllPluginPermissions();
});

// Tests that a plugin in a div that goes from style="display: none" to
// "display: block" can be clicked to activate.
add_task(async function() {
  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_hidden_to_visible.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 20a, Should have a click-to-play notification");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = plugin.openOrClosedShadowRoot.getElementById("main");
    Assert.ok(!!overlay, "Test 20a, Plugin overlay should exist");
  });

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let mainBox = plugin.openOrClosedShadowRoot.getElementById("main");
    let overlayRect = mainBox.getBoundingClientRect();
    Assert.ok(overlayRect.width == 0 && overlayRect.height == 0,
      "Test 20a, plugin should have an overlay with 0px width and height");
  });

  let pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 20b, plugin should not be activated");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let div = doc.getElementById("container");
    Assert.equal(div.style.display, "none",
      "Test 20b, container div should be display: none");
  });

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let div = doc.getElementById("container");
    div.style.display = "block";
  });

  await ContentTask.spawn(gTestBrowser, null, async function() {
    // Waiting for layout to flush and the overlay layout to compute
    await new Promise(resolve => content.requestAnimationFrame(resolve));
    await new Promise(resolve => content.requestAnimationFrame(resolve));

    let doc = content.document;
    let plugin = doc.getElementById("test");
    let mainBox = plugin.openOrClosedShadowRoot.getElementById("main");
    let overlayRect = mainBox.getBoundingClientRect();
    Assert.ok(overlayRect.width == 200 && overlayRect.height == 200,
      "Test 20c, plugin should have overlay dims of 200px");
  });

  pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 20b, plugin should not be activated");

  ok(notification.dismissed, "Test 20c, Doorhanger should start out dismissed");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let bounds = plugin.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.windowUtils;
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
  });

  let condition = () => !notification.dismissed && !!PopupNotifications.panel.firstElementChild;
  await promiseForCondition(condition);
  PopupNotifications.panel.firstElementChild.button.click();

  pluginInfo = await promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 20c, plugin should be activated");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    Assert.ok(!plugin.openOrClosedShadowRoot,
      "Test 20c, CTP UA Widget Shadow Root is removed");
  });

  clearAllPluginPermissions();
});

// Tests that a click-to-play plugin resets its activated state when changing types
add_task(async function() {
  clearAllPluginPermissions();

  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 22, Should have a click-to-play notification");

  // Plugin should start as CTP
  let pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
     "Test 23, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    plugin.type = null;
    // We currently don't properly change state just on type change,
    // so rebind the plugin to tree. bug 767631
    plugin.parentNode.appendChild(plugin);
  });

  pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.displayedType, Ci.nsIObjectLoadingContent.TYPE_NULL, "Test 23, plugin should be TYPE_NULL");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    plugin.type = "application/x-test";
    plugin.parentNode.appendChild(plugin);
  });

  pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.displayedType, Ci.nsIObjectLoadingContent.TYPE_NULL, "Test 23, plugin should be TYPE_NULL");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
     "Test 23, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");
  ok(!pluginInfo.activated, "Test 23, plugin node should not be activated");
});

// Plugin sync removal test. Note this test produces a notification drop down since
// the plugin we add has zero dims.
add_task(async function blockPluginSyncRemoved() {
  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_syncRemoved.html");

  // Maybe there some better trick here, we need to wait for the page load, then
  // wait for the js to execute in the page.
  await waitForMs(500);

  let notification = PopupNotifications.getNotification("click-to-play-plugins");
  ok(notification, "Test 25: There should be a plugin notification even if the plugin was immediately removed");
  ok(notification.dismissed, "Test 25: The notification should be dismissed by default");

  await promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html>hi</html>");
});

// Tests a page with a blocked plugin in it and make sure the infoURL property
// the blocklist file gets used.
add_task(async function blockPluginInfoURL() {
  clearAllPluginPermissions();

  await asyncSetAndUpdateBlocklist(gTestRoot + "blockPluginInfoURL", gTestBrowser);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  info("Waiting for plugin bindings");
  await promiseUpdatePluginBindings(gTestBrowser);

  let notification = PopupNotifications.getNotification("click-to-play-plugins");

  info("Waiting for notification to be shown");
  // Since the plugin notification is dismissed by default, reshow it.
  await promiseForNotificationShown(notification);

  info("Waiting for plugin info");
  let pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_BLOCKLISTED,
     "Test 26, plugin fallback type should be PLUGIN_BLOCKLISTED");
  ok(!pluginInfo.activated, "Plugin should be activated.");
});
