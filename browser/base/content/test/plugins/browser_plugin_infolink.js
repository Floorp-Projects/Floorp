let gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
let gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);
let gTestBrowser = null;

add_task(function* () {
  registerCleanupFunction(function () {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("plugins.click_to_play");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    gTestBrowser = null;
    gBrowser.removeCurrentTab();
    window.focus();
  });
});

add_task(function* () {
  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;

  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_DISABLED, "Test Plug-in");

  // Prime the blocklist service, the remote service doesn't launch on startup.
  yield promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html></html>");
  let exmsg = yield promiseInitContentBlocklistSvc(gBrowser.selectedBrowser);
  ok(!exmsg, "exception: " + exmsg);

  yield asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);
});

add_task(function* () {
  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  yield promiseUpdatePluginBindings(gTestBrowser);

  let pluginInfo = yield promiseForPluginInfo("test");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_DISABLED,
     "Test 1a, plugin fallback type should be PLUGIN_DISABLED");

  // This test opens a new tab to about:addons
  let promise = waitForEvent(gBrowser.tabContainer, "TabOpen", null, true);
  let success = yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let pluginNode = content.document.getElementById("test");
    let manageLink = content.document.getAnonymousElementByAttribute(pluginNode, "anonid", "managePluginsLink");
    let bounds = manageLink.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
    return true;
  });
  ok(success, "click on manage link");
  yield promise;

  promise = waitForEvent(gBrowser.tabContainer, "TabClose", null, true);

  // in-process page, no cpows here
  let condition = function() {
    let win = gBrowser.selectedBrowser.contentWindow;
    if (!!win && !!win.wrappedJSObject && !!win.wrappedJSObject.gViewController) {
      return win.wrappedJSObject.gViewController.currentViewId == "addons://list/plugin";
    }
    return false;
  }

  yield promiseForCondition(condition, "Waited too long for about:addons to display.", 40, 500);

  // remove the tab containing about:addons
  gBrowser.removeCurrentTab();

  yield promise;
});

