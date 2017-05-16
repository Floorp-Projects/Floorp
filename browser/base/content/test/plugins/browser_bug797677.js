var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;
var gConsoleErrors = 0;

var Cc = Components.classes;
var Ci = Components.interfaces;

add_task(async function() {
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    consoleService.unregisterListener(errorListener);
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gTestBrowser = gBrowser.selectedBrowser;

  let consoleService = Cc["@mozilla.org/consoleservice;1"]
                         .getService(Ci.nsIConsoleService);
  let errorListener = {
    observe(aMessage) {
      if (aMessage.message.includes("NS_ERROR_FAILURE"))
        gConsoleErrors++;
    }
  };
  consoleService.registerListener(errorListener);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_bug797677.html");

  let pluginInfo = await promiseForPluginInfo("plugin");
  is(pluginInfo.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_UNSUPPORTED, "plugin should not have been found.");

  // simple cpows
  await ContentTask.spawn(gTestBrowser, null, function() {
    let plugin = content.document.getElementById("plugin");
    ok(plugin, "plugin should be in the page");
  });
  is(gConsoleErrors, 0, "should have no console errors");
});
