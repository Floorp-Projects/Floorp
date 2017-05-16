var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);
var gTestBrowser = null;

add_task(async function() {
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    Services.prefs.clearUserPref("plugins.click_to_play");
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });

  gBrowser.selectedTab =  BrowserTestUtils.addTab(gBrowser);
  gTestBrowser = gBrowser.selectedBrowser;

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Second Test Plug-in");
});

// Test that the click-to-play doorhanger still works when navigating to data URLs
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_data_url.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 1a, Should have a click-to-play notification");

  let pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 1a, plugin should not be activated");

  let loadPromise = promiseTabLoadEvent(gBrowser.selectedTab);
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    // navigate forward to a page with 'test' in it
    content.document.getElementById("data-link-1").click();
  });
  await loadPromise;

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 1b, Should have a click-to-play notification");

  pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 1b, plugin should not be activated");

  let promise = promisePopupNotification("click-to-play-plugins");
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let plugin = content.document.getElementById("test");
    let bounds = plugin.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
  });
  await promise;

  // Simulate clicking the "Allow Always" button.
  let condition = () => !PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed &&
    PopupNotifications.panel.firstChild;
  await promiseForCondition(condition);
  PopupNotifications.panel.firstChild._primaryButton.click();

  // check plugin state
  pluginInfo = await promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 1b, plugin should be activated");
});

// Test that the click-to-play notification doesn't break when navigating
// to data URLs with multiple plugins.
add_task(async function() {
  // We click activated above
  clearAllPluginPermissions();

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_data_url.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 2a, Should have a click-to-play notification");

  let pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 2a, plugin should not be activated");

  let loadPromise = promiseTabLoadEvent(gBrowser.selectedTab);
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    // navigate forward to a page with 'test1' & 'test2' in it
    content.document.getElementById("data-link-2").click();
  });
  await loadPromise;

  // Work around for delayed PluginBindingAttached
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    content.document.getElementById("test1").clientTop;
    content.document.getElementById("test2").clientTop;
  });

  pluginInfo = await promiseForPluginInfo("test1");
  ok(!pluginInfo.activated, "Test 2a, test1 should not be activated");
  pluginInfo = await promiseForPluginInfo("test2");
  ok(!pluginInfo.activated, "Test 2a, test2 should not be activated");

  notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 2b, Should have a click-to-play notification");

  await promiseForNotificationShown(notification);

  // Simulate choosing "Allow now" for the test plugin
  is(notification.options.pluginData.size, 2, "Test 2b, Should have two types of plugin in the notification");

  let centerAction = null;
  for (let action of notification.options.pluginData.values()) {
    if (action.pluginName == "Test") {
      centerAction = action;
      break;
    }
  }
  ok(centerAction, "Test 2b, found center action for the Test plugin");

  let centerItem = null;
  for (let item of PopupNotifications.panel.firstChild.childNodes) {
    is(item.value, "block", "Test 2b, all plugins should start out blocked");
    if (item.action == centerAction) {
      centerItem = item;
      break;
    }
  }
  ok(centerItem, "Test 2b, found center item for the Test plugin");

  // "click" the button to activate the Test plugin
  centerItem.value = "allownow";
  PopupNotifications.panel.firstChild._primaryButton.click();

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  // check plugin state
  pluginInfo = await promiseForPluginInfo("test1");
  ok(pluginInfo.activated, "Test 2b, plugin should be activated");
});

add_task(async function() {
  // We click activated above
  clearAllPluginPermissions();

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_data_url.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);
});

// Test that when navigating to a data url, the plugin permission is inherited
add_task(async function() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 3a, Should have a click-to-play notification");

  // check plugin state
  let pluginInfo = await promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 3a, plugin should not be activated");

  let promise = promisePopupNotification("click-to-play-plugins");
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let plugin = content.document.getElementById("test");
    let bounds = plugin.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
  });
  await promise;

  // Simulate clicking the "Allow Always" button.
  let condition = () => !PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed &&
    PopupNotifications.panel.firstChild;
  await promiseForCondition(condition);
  PopupNotifications.panel.firstChild._primaryButton.click();

  // check plugin state
  pluginInfo = await promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 3a, plugin should be activated");

  let loadPromise = promiseTabLoadEvent(gBrowser.selectedTab);
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    // navigate forward to a page with 'test' in it
    content.document.getElementById("data-link-1").click();
  });
  await loadPromise;

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  // check plugin state
  pluginInfo = await promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 3b, plugin should be activated");

  clearAllPluginPermissions();
});

// Test that the click-to-play doorhanger still works
// when directly navigating to data URLs.
// Fails, bug XXX. Plugins plus a data url don't fire a load event.
/*
add_task(function* () {
  yield promiseTabLoadEvent(gBrowser.selectedTab,
   "data:text/html,Hi!<embed id='test' style='width:200px; height:200px' type='application/x-test'/>");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 4a, Should have a click-to-play notification");

  // check plugin state
  let pluginInfo = yield promiseForPluginInfo("test");
  ok(!pluginInfo.activated, "Test 4a, plugin should not be activated");

  let promise = promisePopupNotification("click-to-play-plugins");
  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let plugin = content.document.getElementById("test");
    let bounds = plugin.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
  });
  yield promise;

  // Simulate clicking the "Allow Always" button.
  let condition = () => !PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed &&
    PopupNotifications.panel.firstChild;
  yield promiseForCondition(condition);
  PopupNotifications.panel.firstChild._primaryButton.click();

  // check plugin state
  pluginInfo = yield promiseForPluginInfo("test");
  ok(pluginInfo.activated, "Test 4a, plugin should be activated");
});
*/
