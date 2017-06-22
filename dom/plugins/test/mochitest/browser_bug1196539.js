var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

function checkPaintCount(aCount) {
  ok(aCount != 0, "paint count can't be greater than zero, count was " + aCount);
  ok(aCount < kMaxPaints, "paint count should be within limits, count was " + aCount);
}

// maximum number of paints we allow before failing. The test plugin doesn't
// animate so this should really be just 1, but operating systems can
// occasionally fire a few of these so we give these tests a fudge factor.
// A bad regression would either be 0, or 100+.
const kMaxPaints = 10;

add_task(async function() {
  let result, tabSwitchedPromise;

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");

  let testTab = gBrowser.selectedTab;
  let pluginTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, gTestRoot + "plugin_test.html");
  let homeTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home");

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return !!plugin;
  });
  is(result, true, "plugin is loaded");

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return !XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin is hidden");

  // reset plugin paint count
  await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    XPCNativeWrapper.unwrap(plugin).resetPaintCount();
  });

  // select plugin tab
  tabSwitchedPromise = waitTabSwitched();
  gBrowser.selectedTab = pluginTab;
  await tabSwitchedPromise;

  // wait a bit for spurious paints
  await waitForMs(100);

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin is visible");

  // check for good paint count
  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).getPaintCount();
  });
  checkPaintCount(result);

  // select home tab
  tabSwitchedPromise = waitTabSwitched();
  gBrowser.selectedTab = homeTab;
  await tabSwitchedPromise;

  // reset paint count
  await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    XPCNativeWrapper.unwrap(plugin).resetPaintCount();
  });

  // wait a bit for spurious paints
  await waitForMs(100);

  // check for no paint count
  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).getPaintCount();
  });
  is(result, 0, "no paints, this is correct.");

  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return !XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
  });
  is(result, true, "plugin is hidden");

  // reset paint count
  await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    XPCNativeWrapper.unwrap(plugin).resetPaintCount();
  });

  // select plugin tab
  tabSwitchedPromise = waitTabSwitched();
  gBrowser.selectedTab = pluginTab;
  await tabSwitchedPromise;

  // check paint count
  result = await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return XPCNativeWrapper.unwrap(plugin).getPaintCount();
  });
  checkPaintCount(result);

  gBrowser.removeTab(homeTab);
  gBrowser.removeTab(pluginTab);
});
