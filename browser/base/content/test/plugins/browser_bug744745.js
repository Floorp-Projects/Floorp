var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;
var gNumPluginBindingsAttached = 0;

function pluginBindingAttached() {
  gNumPluginBindingsAttached++;
  if (gNumPluginBindingsAttached != 1) {
    ok(false, "if we've gotten here, something is quite wrong");
  }
}

add_task(async function() {
  registerCleanupFunction(function() {
    gTestBrowser.removeEventListener("PluginBindingAttached", pluginBindingAttached, true, true);
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("plugins.click_to_play");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });
});

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gTestBrowser = gBrowser.selectedBrowser;

  Services.prefs.setBoolPref("plugins.click_to_play", true);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  BrowserTestUtils.addContentEventListener(gTestBrowser, "PluginBindingAttached", pluginBindingAttached, true, null, true);

  let testRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
  await promiseTabLoadEvent(gBrowser.selectedTab, testRoot + "plugin_bug744745.html");

  await promiseForCondition(function() { return gNumPluginBindingsAttached == 1; });

  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let plugin = content.document.getElementById("test");
    if (!plugin) {
      Assert.ok(false, "plugin element not available.");
      return;
    }
    // We can't use MochiKit's routine
    let style = content.getComputedStyle(plugin);
    Assert.ok(("opacity" in style) && style.opacity == 1, "plugin style properly configured.");
  });
});
