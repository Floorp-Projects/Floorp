var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;

add_task(async function() {
  registerCleanupFunction(async function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("plugins.click_to_play");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins", gTestBrowser);
    resetBlocklist();
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });
});

add_task(async function() {
  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;

  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Second Test Plug-in");

  // Prime the blocklist service, the remote service doesn't launch on startup.
  await promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html></html>");

  await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins", gTestBrowser);
});

// Tests that navigation within the page and the window.history API doesn't break click-to-play state.
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_add_dynamically.html");

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!notification, "Test 1a, Should not have a click-to-play notification");

  await ContentTask.spawn(gTestBrowser, {}, async function() {
    new XPCNativeWrapper(XPCNativeWrapper.unwrap(content).addPlugin());
  });

  await promisePopupNotification("click-to-play-plugins");
});

add_task(async function() {
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let plugin = content.document.getElementsByTagName("embed")[0];
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.ok(!objLoadingContent.activated, "Test 1b, Plugin should not be activated");
  });

  // Click the activate button on doorhanger to make sure it works
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);

  await promiseForNotificationShown(notification);

  PopupNotifications.panel.firstElementChild.button.click();

  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let plugin = content.document.getElementsByTagName("embed")[0];
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.ok(objLoadingContent.activated, "Test 1b, Plugin should be activated");
  });
});

add_task(async function() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 1c, Should still have a click-to-play notification");

  await ContentTask.spawn(gTestBrowser, {}, async function() {
    new XPCNativeWrapper(XPCNativeWrapper.unwrap(content).addPlugin());
    let plugin = content.document.getElementsByTagName("embed")[1];
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.ok(objLoadingContent.activated,
      "Test 1c, Newly inserted plugin in activated page should be activated");
  });
});

add_task(async function() {
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let plugin = content.document.getElementsByTagName("embed")[1];
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.ok(objLoadingContent.activated, "Test 1d, Plugin should be activated");

    let promise = ContentTaskUtils.waitForEvent(content, "hashchange");
    content.location += "#anchorNavigation";
    await promise;
  });
});

add_task(async function() {
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    new XPCNativeWrapper(XPCNativeWrapper.unwrap(content).addPlugin());
    let plugin = content.document.getElementsByTagName("embed")[2];
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.ok(objLoadingContent.activated, "Test 1e, Plugin should be activated");
  });
});

add_task(async function() {
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let plugin = content.document.getElementsByTagName("embed")[2];
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.ok(objLoadingContent.activated, "Test 1f, Plugin should be activated");

    content.history.replaceState({}, "", "replacedState");
    new XPCNativeWrapper(XPCNativeWrapper.unwrap(content).addPlugin());
    plugin = content.document.getElementsByTagName("embed")[3];
    objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.ok(objLoadingContent.activated, "Test 1g, Plugin should be activated");
  });
});
