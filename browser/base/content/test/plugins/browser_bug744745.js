var gTestRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://127.0.0.1:8888/"
);
var gTestBrowser = null;

add_task(async function() {
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(
      Ci.nsIPluginTag.STATE_ENABLED,
      "Second Test Plug-in"
    );
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });
});

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gTestBrowser = gBrowser.selectedBrowser;

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  let promisePluginBindingAttached = BrowserTestUtils.waitForContentEvent(
    gTestBrowser,
    "PluginBindingAttached",
    true,
    null,
    true
  );

  await promiseTabLoadEvent(
    gBrowser.selectedTab,
    gTestRoot + "plugin_bug744745.html"
  );

  await promisePluginBindingAttached;

  await SpecialPowers.spawn(gTestBrowser, [], async function() {
    let plugin = content.document.getElementById("test");
    if (!plugin) {
      Assert.ok(false, "plugin element not available.");
      return;
    }
    // We can't use MochiKit's routine
    let style = content.getComputedStyle(plugin);
    Assert.ok(
      "opacity" in style && style.opacity == 1,
      "plugin style properly configured."
    );
  });
});
