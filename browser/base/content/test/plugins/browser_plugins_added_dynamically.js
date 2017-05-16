var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://mochi.test:8888/");
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);

var gTestBrowser = null;

add_task(async function() {
  registerCleanupFunction(async function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("plugins.click_to_play");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);
    resetBlocklist();
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });
});

// "Activate" of a given type -> plugins of that type dynamically added should
// automatically play.
add_task(async function() {
  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;

  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Second Test Plug-in");
});

add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_add_dynamically.html");

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!notification, "Test 1a, Should not have a click-to-play notification");

  // Add a plugin of type test
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    new XPCNativeWrapper(XPCNativeWrapper.unwrap(content).addPlugin("pluginone", "application/x-test"));
  });

  await promisePopupNotification("click-to-play-plugins");

  notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 1a, Should not have a click-to-play notification");

  await promiseForNotificationShown(notification);

  let centerAction = null;
  for (let action of notification.options.pluginData.values()) {
    if (action.pluginName == "Test") {
      centerAction = action;
      break;
    }
  }
  ok(centerAction, "Test 2, found center action for the Test plugin");

  let centerItem = null;
  for (let item of PopupNotifications.panel.firstChild.childNodes) {
    is(item.value, "block", "Test 3, all plugins should start out blocked");
    if (item.action == centerAction) {
      centerItem = item;
      break;
    }
  }
  ok(centerItem, "Test 4, found center item for the Test plugin");

  // Select the allow now option in the select drop down for Test Plugin
  centerItem.value = "allownow";

  // "click" the button to activate the Test plugin
  PopupNotifications.panel.firstChild._primaryButton.click();

  let pluginInfo = await promiseForPluginInfo("pluginone");
  ok(pluginInfo.activated, "Test 5, plugin should be activated");

  // Add another plugin of type test
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    new XPCNativeWrapper(XPCNativeWrapper.unwrap(content).addPlugin("plugintwo", "application/x-test"));
  });

  pluginInfo = await promiseForPluginInfo("plugintwo");
  ok(pluginInfo.activated, "Test 6, plugins should be activated");
});

// "Activate" of a given type -> plugins of other types dynamically added
// should not automatically play.
add_task(async function() {
  clearAllPluginPermissions();

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_add_dynamically.html");

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!notification, "Test 7, Should not have a click-to-play notification");

  // Add a plugin of type test
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    new XPCNativeWrapper(XPCNativeWrapper.unwrap(content).addPlugin("pluginone", "application/x-test"));
  });

  await promisePopupNotification("click-to-play-plugins");

  notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 8, Should not have a click-to-play notification");

  await promiseForNotificationShown(notification);

  is(notification.options.pluginData.size, 1, "Should be one plugin action");

  let pluginInfo = await promiseForPluginInfo("pluginone");
  ok(!pluginInfo.activated, "Test 8, test plugin should be activated");

  let condition = () => !notification.dismissed &&
    PopupNotifications.panel.firstChild;
  await promiseForCondition(condition);

  // "click" the button to activate the Test plugin
  PopupNotifications.panel.firstChild._primaryButton.click();

  pluginInfo = await promiseForPluginInfo("pluginone");
  ok(pluginInfo.activated, "Test 9, test plugin should be activated");

  await ContentTask.spawn(gTestBrowser, {}, async function() {
    new XPCNativeWrapper(XPCNativeWrapper.unwrap(content).addPlugin("plugintwo", "application/x-second-test"));
  });

  await promisePopupNotification("click-to-play-plugins");

  pluginInfo = await promiseForPluginInfo("pluginone");
  ok(pluginInfo.activated, "Test 10, plugins should be activated");
  pluginInfo = await promiseForPluginInfo("plugintwo");
  ok(!pluginInfo.activated, "Test 11, plugins should be activated");
});
