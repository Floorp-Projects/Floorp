var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);

function updateAllTestPlugins(aState) {
  setTestPluginEnabledState(aState, "Test Plug-in");
  setTestPluginEnabledState(aState, "Second Test Plug-in");
}

add_task(async function() {
  registerCleanupFunction(async function() {
    clearAllPluginPermissions();
    updateAllTestPlugins(Ci.nsIPluginTag.STATE_ENABLED);
    Services.prefs.clearUserPref("plugins.click_to_play");
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);
    resetBlocklist();
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });
});

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gTestBrowser = gBrowser.selectedBrowser;

  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  // Prime the content process
  await promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html>hi</html>");

  // Make sure the blocklist service(s) are running
  Components.classes["@mozilla.org/extensions/blocklist;1"]
            .getService(Components.interfaces.nsIBlocklistService);
  let exmsg = await promiseInitContentBlocklistSvc(gBrowser.selectedBrowser);
  ok(!exmsg, "exception: " + exmsg);
});

add_task(async function() {
  // enable hard blocklisting for the next test
  await asyncSetAndUpdateBlocklist(gTestRoot + "blockPluginHard.xml", gTestBrowser);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await promisePopupNotification("click-to-play-plugins");

  let notification = PopupNotifications.getNotification("click-to-play-plugins");
  ok(notification.dismissed, "Test 5: The plugin notification should be dismissed by default");

  await promiseForNotificationShown(notification);

  let pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_BLOCKLISTED, "Test 5, plugin fallback type should be PLUGIN_BLOCKLISTED");

  is(notification.options.pluginData.size, 1, "Test 5: Only the blocked plugin should be present in the notification");
  ok(PopupNotifications.panel.firstChild._buttonContainer.hidden, "Part 5: The blocked plugins notification should not have any buttons visible.");

  await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);
});

// Tests a vulnerable, updatable plugin

add_task(async function() {
  // enable hard blocklisting of test
  await asyncSetAndUpdateBlocklist(gTestRoot + "blockPluginVulnerableUpdatable.xml", gTestBrowser);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await promisePopupNotification("click-to-play-plugins");

  let pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE,
     "Test 18a, plugin fallback type should be PLUGIN_VULNERABLE_UPDATABLE");
  ok(!pluginInfo.activated, "Test 18a, Plugin should not be activated");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(overlay && overlay.classList.contains("visible"),
      "Test 18a, Plugin overlay should exist, not be hidden");

    let updateLink = doc.getAnonymousElementByAttribute(plugin, "anonid", "checkForUpdatesLink");
    Assert.ok(updateLink.style.visibility != "hidden",
      "Test 18a, Plugin should have an update link");
  });

  let promise = waitForEvent(gBrowser.tabContainer, "TabOpen", null, true);

  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let updateLink = doc.getAnonymousElementByAttribute(plugin, "anonid", "checkForUpdatesLink");
    let bounds = updateLink.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
  });
  await promise;

  promise = waitForEvent(gBrowser.tabContainer, "TabClose", null, true);
  gBrowser.removeCurrentTab();
  await promise;
});

add_task(async function() {
  // clicking the update link should not activate the plugin
  let pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE,
     "Test 18a, plugin fallback type should be PLUGIN_VULNERABLE_UPDATABLE");
  ok(!pluginInfo.activated, "Test 18b, Plugin should not be activated");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(overlay && overlay.classList.contains("visible"),
      "Test 18b, Plugin overlay should exist, not be hidden");
  });
});

// Tests a vulnerable plugin with no update
add_task(async function() {
  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  await asyncSetAndUpdateBlocklist(gTestRoot + "blockPluginVulnerableNoUpdate.xml", gTestBrowser);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 18c, Should have a click-to-play notification");

  let pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE,
     "Test 18c, plugin fallback type should be PLUGIN_VULNERABLE_NO_UPDATE");
  ok(!pluginInfo.activated, "Test 18c, Plugin should not be activated");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(overlay && overlay.classList.contains("visible"),
      "Test 18c, Plugin overlay should exist, not be hidden");

    let updateLink = doc.getAnonymousElementByAttribute(plugin, "anonid", "checkForUpdatesLink");
    Assert.ok(updateLink && updateLink.style.display != "block",
      "Test 18c, Plugin should not have an update link");
  });

  // check that click "Always allow" works with blocked plugins
  await promiseForNotificationShown(notification);

  PopupNotifications.panel.firstChild._primaryButton.click();

  pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE,
     "Test 18c, plugin fallback type should be PLUGIN_VULNERABLE_NO_UPDATE");
  ok(pluginInfo.activated, "Test 18c, Plugin should be activated");
  let enabledState = getTestPluginEnabledState();
  ok(enabledState, "Test 18c, Plugin enabled state should be STATE_CLICKTOPLAY");
});

// continue testing "Always allow", make sure it sticks.
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let pluginInfo = await promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 18d, Waited too long for plugin to activate");

  clearAllPluginPermissions();
});

// clicking the in-content overlay of a vulnerable plugin should bring
// up the notification and not directly activate the plugin
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 18f, Should have a click-to-play notification");
  ok(notification.dismissed, "Test 18f, notification should start dismissed");

  let pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 18f, Waited too long for plugin to activate");

  var oldEventCallback = notification.options.eventCallback;
  let promise = promiseForCondition(() => oldEventCallback == null);
  notification.options.eventCallback = function() {
    if (oldEventCallback) {
      oldEventCallback();
    }
    oldEventCallback = null;
  };

  await ContentTask.spawn(gTestBrowser, {}, async function() {
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
  await promise;

  ok(notification, "Test 18g, Should have a click-to-play notification");
  ok(!notification.dismissed, "Test 18g, notification should be open");

  pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 18g, Plugin should not be activated");
});

// Test that "always allow"-ing a plugin will not allow it when it becomes
// blocklisted.
add_task(async function() {
  await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);

  updateAllTestPlugins(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 24a, Should have a click-to-play notification");

  // Plugin should start as CTP
  let pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
     "Test 24a, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");
  ok(!pluginInfo.activated, "Test 24a, Plugin should not be active.");

  // simulate "always allow"
  await promiseForNotificationShown(notification);

  PopupNotifications.panel.firstChild._primaryButton.click();

  pluginInfo = await promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 24a, Plugin should be active.");

  await asyncSetAndUpdateBlocklist(gTestRoot + "blockPluginVulnerableUpdatable.xml", gTestBrowser);
});

// the plugin is now blocklisted, so it should not automatically load
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 24b, Should have a click-to-play notification");

  let pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE,
     "Test 24b, plugin fallback type should be PLUGIN_VULNERABLE_UPDATABLE");
  ok(!pluginInfo.activated, "Test 24b, Plugin should not be active.");

  // simulate "always allow"
  await promiseForNotificationShown(notification);

  PopupNotifications.panel.firstChild._primaryButton.click();

  pluginInfo = await promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 24b, Plugin should be active.");

  clearAllPluginPermissions();

  await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);
});

// Plugin sync removal test. Note this test produces a notification drop down since
// the plugin we add has zero dims.
add_task(async function() {
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
add_task(async function() {
  clearAllPluginPermissions();

  await asyncSetAndUpdateBlocklist(gTestRoot + "blockPluginInfoURL.xml", gTestBrowser);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let notification = PopupNotifications.getNotification("click-to-play-plugins");

  // Since the plugin notification is dismissed by default, reshow it.
  await promiseForNotificationShown(notification);

  let pluginInfo = await promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_BLOCKLISTED,
     "Test 26, plugin fallback type should be PLUGIN_BLOCKLISTED");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let plugin = content.document.getElementById("test");
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.ok(!objLoadingContent.activated, "Plugin should not be activated.");
  });

  const testUrl = "http://test.url.com/";

  let firstPanelChild = PopupNotifications.panel.firstChild;
  let infoLink = document.getAnonymousElementByAttribute(firstPanelChild, "anonid",
    "click-to-play-plugins-notification-link");
  is(infoLink.href, testUrl,
    "Test 26, the notification URL needs to match the infoURL from the blocklist file.");
});

