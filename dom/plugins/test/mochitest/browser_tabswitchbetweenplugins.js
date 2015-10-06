let gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

// tests that we get plugin updates when we flip between tabs that
// have the same plugin in the same position in the page.

add_task(function* () {
  let result, tabSwitchedPromise;

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");

  let testTab = gBrowser.selectedTab;
  let pluginTab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, gTestRoot + "plugin_test.html");
  let pluginTab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, gTestRoot + "plugin_test.html");

  result = yield ContentTask.spawn(pluginTab1.linkedBrowser, null, function*() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return !!plugin;
  });
  is(result, true, "plugin1 is loaded");

  result = yield ContentTask.spawn(pluginTab2.linkedBrowser, null, function*() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return !!plugin;
  });
  is(result, true, "plugin2 is loaded");

  // plugin tab 2 should be selected
  is(gBrowser.selectedTab == pluginTab2, true, "plugin2 is selected");

  result = yield ContentTask.spawn(pluginTab1.linkedBrowser, null, function*() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, false, "plugin1 is hidden");

  result = yield ContentTask.spawn(pluginTab2.linkedBrowser, null, function*() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin2 is visible");

  // select plugin1 tab
  tabSwitchedPromise = waitTabSwitched();
  gBrowser.selectedTab = pluginTab1;
  yield tabSwitchedPromise;

  result = yield ContentTask.spawn(pluginTab1.linkedBrowser, null, function*() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin1 is visible");

  result = yield ContentTask.spawn(pluginTab2.linkedBrowser, null, function*() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, false, "plugin2 is hidden");

  // select plugin2 tab
  tabSwitchedPromise = waitTabSwitched();
  gBrowser.selectedTab = pluginTab2;
  yield tabSwitchedPromise;

  result = yield ContentTask.spawn(pluginTab1.linkedBrowser, null, function*() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, false, "plugin1 is hidden");

  result = yield ContentTask.spawn(pluginTab2.linkedBrowser, null, function*() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin2 is visible");

  // select test tab
  tabSwitchedPromise = waitTabSwitched();
  gBrowser.selectedTab = testTab;
  yield tabSwitchedPromise;

  result = yield ContentTask.spawn(pluginTab1.linkedBrowser, null, function*() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, false, "plugin1 is hidden");

  result = yield ContentTask.spawn(pluginTab2.linkedBrowser, null, function*() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, false, "plugin2 is hidden");

  gBrowser.removeTab(pluginTab1);
  gBrowser.removeTab(pluginTab2);
});
