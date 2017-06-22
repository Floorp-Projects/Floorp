var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

/**
 * tests for plugin windows and scroll
 */

function coordinatesRelativeToWindow(aX, aY, aElement) {
  var targetWindow = aElement.ownerGlobal;
  var scale = targetWindow.devicePixelRatio;
  var rect = aElement.getBoundingClientRect();
  return {
    x: targetWindow.mozInnerScreenX + ((rect.left + aX) * scale),
    y: targetWindow.mozInnerScreenY + ((rect.top + aY) * scale)
  };
}

var apzEnabled = Preferences.get("layers.async-pan-zoom.enabled", false);
var pluginHideEnabled = Preferences.get("gfx.e10s.hide-plugins-for-scroll", true);


add_task(async function() {
  registerCleanupFunction(function () {
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  });
});

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    "set": [
             ["general.smoothScroll", true],
             ["general.smoothScroll.other", true],
             ["general.smoothScroll.mouseWheel", true],
             ["general.smoothScroll.other.durationMaxMS", 2000],
             ["general.smoothScroll.other.durationMinMS", 1999],
             ["general.smoothScroll.mouseWheel.durationMaxMS", 2000],
             ["general.smoothScroll.mouseWheel.durationMinMS", 1999],
           ]});
});

/*
 * test plugin visibility when scrolling with scroll wheel and apz in a top level document.
 */

add_task(async function() {
  let result;

  if (!apzEnabled) {
    ok(true, "nothing to test, need apz");
    return;
  }

  if (!pluginHideEnabled) {
    ok(true, "nothing to test, need gfx.e10s.hide-plugins-for-scroll");
    return;
  }

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");

  let testTab = gBrowser.selectedTab;
  let pluginTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, gTestRoot + "plugin_test.html");

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return !!plugin;
  });
  is(result, true, "plugin is loaded");

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin is visible");

  let nativeId = nativeVerticalWheelEventMsg();
  let utils = SpecialPowers.getDOMWindowUtils(window);
  let screenCoords = coordinatesRelativeToWindow(10, 10,
                                                 gBrowser.selectedBrowser);
  utils.sendNativeMouseScrollEvent(screenCoords.x, screenCoords.y,
                                   nativeId, 0, -50, 0, 0, 0,
                                   gBrowser.selectedBrowser);

  await waitScrollStart(gBrowser.selectedBrowser);

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, false, "plugin is hidden");

  await waitScrollFinish(gBrowser.selectedBrowser);

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin is visible");

  gBrowser.removeTab(pluginTab);
});

/*
 * test plugin visibility when scrolling with scroll wheel and apz in a sub document.
 */

add_task(async function() {
  let result;

  if (!apzEnabled) {
    ok(true, "nothing to test, need apz");
    return;
  }

  if (!pluginHideEnabled) {
    ok(true, "nothing to test, need gfx.e10s.hide-plugins-for-scroll");
    return;
  }

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");

  let testTab = gBrowser.selectedTab;
  let pluginTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, gTestRoot + "plugin_subframe_test.html");

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document.getElementById("subframe").contentDocument;
    let plugin = doc.getElementById("testplugin");
    return !!plugin;
  });
  is(result, true, "plugin is loaded");

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document.getElementById("subframe").contentDocument;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin is visible");

  let nativeId = nativeVerticalWheelEventMsg();
  let utils = SpecialPowers.getDOMWindowUtils(window);
  let screenCoords = coordinatesRelativeToWindow(10, 10,
                                                 gBrowser.selectedBrowser);
  utils.sendNativeMouseScrollEvent(screenCoords.x, screenCoords.y,
                                   nativeId, 0, -50, 0, 0, 0,
                                   gBrowser.selectedBrowser);

  await waitScrollStart(gBrowser.selectedBrowser);

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document.getElementById("subframe").contentDocument;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, false, "plugin is hidden");

  await waitScrollFinish(gBrowser.selectedBrowser);

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document.getElementById("subframe").contentDocument;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin is visible");

  gBrowser.removeTab(pluginTab);
});

/*
 * test visibility when scrolling with keyboard shortcuts for a top level document.
 * This circumvents apz and relies on dom scroll, which is what we want to target
 * for this test.
 */

add_task(async function() {
  let result;

  if (!pluginHideEnabled) {
    ok(true, "nothing to test, need gfx.e10s.hide-plugins-for-scroll");
    return;
  }

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");

  let testTab = gBrowser.selectedTab;
  let pluginTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, gTestRoot + "plugin_test.html");

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return !!plugin;
  });
  is(result, true, "plugin is loaded");

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin is visible");

  EventUtils.synthesizeKey("VK_END", {});

  await waitScrollStart(gBrowser.selectedBrowser);

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, false, "plugin is hidden");

  await waitScrollFinish(gBrowser.selectedBrowser);

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, false, "plugin is hidden");

  EventUtils.synthesizeKey("VK_HOME", {});

  await waitScrollFinish(gBrowser.selectedBrowser);

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin is visible");

  gBrowser.removeTab(pluginTab);
});

/*
 * test visibility when scrolling with keyboard shortcuts for a sub document.
 */

add_task(async function() {
  let result;

  if (!pluginHideEnabled) {
    ok(true, "nothing to test, need gfx.e10s.hide-plugins-for-scroll");
    return;
  }

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");

  let testTab = gBrowser.selectedTab;
  let pluginTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, gTestRoot + "plugin_subframe_test.html");

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document.getElementById("subframe").contentDocument;
    let plugin = doc.getElementById("testplugin");
    return !!plugin;
  });
  is(result, true, "plugin is loaded");

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document.getElementById("subframe").contentDocument;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin is visible");

  EventUtils.synthesizeKey("VK_END", {});

  await waitScrollStart(gBrowser.selectedBrowser);

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document.getElementById("subframe").contentDocument;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, false, "plugin is hidden");

  await waitScrollFinish(gBrowser.selectedBrowser);

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document.getElementById("subframe").contentDocument;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, false, "plugin is hidden");

  EventUtils.synthesizeKey("VK_HOME", {});

  await waitScrollFinish(gBrowser.selectedBrowser);

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document.getElementById("subframe").contentDocument;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin is visible");

  gBrowser.removeTab(pluginTab);
});
