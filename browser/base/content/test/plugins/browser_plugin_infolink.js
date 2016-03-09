var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);
var oldBrowserOpenAddonsMgr = window.BrowserOpenAddonsMgr;

registerCleanupFunction(function* cleanup() {
  clearAllPluginPermissions();
  Services.prefs.clearUserPref("plugins.click_to_play");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
  window.BrowserOpenAddonsMgr = oldBrowserOpenAddonsMgr;
  window.focus();
});

add_task(function* test_clicking_manage_link_in_plugin_overlay_should_open_about_addons() {
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_DISABLED, "Test Plug-in");

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, gTestRoot + "plugin_test.html");
  let browser = tab.linkedBrowser;
  yield promiseUpdatePluginBindings(browser);

  let pluginInfo = yield promiseForPluginInfo("test", browser);
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_DISABLED,
     "plugin fallback type should be PLUGIN_DISABLED");

  let awaitBrowserOpenAddonsMgr = new Promise(resolve => {
    window.BrowserOpenAddonsMgr = function(view) {
      resolve(view);
    }
  });

  yield ContentTask.spawn(browser, null, function* () {
    let pluginNode = content.document.getElementById("test");
    let manageLink = content.document.getAnonymousElementByAttribute(pluginNode, "anonid", "managePluginsLink");
    let bounds = manageLink.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
    Assert.ok(true, "click on manage link");
  });

  let requestedView = yield awaitBrowserOpenAddonsMgr;
  is(requestedView, "addons://list/plugin", "The Add-ons Manager should open the plugin view");

  yield BrowserTestUtils.removeTab(tab);
});
