var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;

add_task(function* () {
  registerCleanupFunction(Task.async(function*() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("plugins.click_to_play");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    yield asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);
    resetBlocklist();
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  }));
});

add_task(function* () {
  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;

  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Second Test Plug-in");

  // Prime the blocklist service, the remote service doesn't launch on startup.
  yield promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html></html>");

  let exmsg = yield promiseInitContentBlocklistSvc(gBrowser.selectedBrowser);
  ok(!exmsg, "exception: " + exmsg);

  yield asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);
});

// Tests that navigation within the page and the window.history API doesn't break click-to-play state.
add_task(function* () {
  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_add_dynamically.html");

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!notification, "Test 1a, Should not have a click-to-play notification");

  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    new XPCNativeWrapper(XPCNativeWrapper.unwrap(content).addPlugin());
  });

  yield promisePopupNotification("click-to-play-plugins");
});

add_task(function* () {
  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let plugin = content.document.getElementsByTagName("embed")[0];
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.ok(!objLoadingContent.activated, "Test 1b, Plugin should not be activated");
  });

  // Click the activate button on doorhanger to make sure it works
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);

  yield promiseForNotificationShown(notification);

  PopupNotifications.panel.firstChild._primaryButton.click();

  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let plugin = content.document.getElementsByTagName("embed")[0];
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.ok(objLoadingContent.activated, "Test 1b, Plugin should be activated");
  });
});

add_task(function* () {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 1c, Should still have a click-to-play notification");

  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    new XPCNativeWrapper(XPCNativeWrapper.unwrap(content).addPlugin());
    let plugin = content.document.getElementsByTagName("embed")[1];
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.ok(objLoadingContent.activated,
      "Test 1c, Newly inserted plugin in activated page should be activated");
  });
});

add_task(function* () {
  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let plugin = content.document.getElementsByTagName("embed")[1];
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.ok(objLoadingContent.activated, "Test 1d, Plugin should be activated");
  });

  let promise = waitForEvent(gTestBrowser.contentWindow, "hashchange", null);
  gTestBrowser.contentWindow.location += "#anchorNavigation";
  yield promise;
});

add_task(function* () {
  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    new XPCNativeWrapper(XPCNativeWrapper.unwrap(content).addPlugin());
    let plugin = content.document.getElementsByTagName("embed")[2];
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Assert.ok(objLoadingContent.activated, "Test 1e, Plugin should be activated");
  });
});

add_task(function* () {
  yield ContentTask.spawn(gTestBrowser, {}, function* () {
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
