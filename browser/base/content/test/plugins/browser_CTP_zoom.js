"use strict";

var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

var gTestBrowser = null;

add_task(async function() {
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    Services.prefs.clearUserPref("plugins.click_to_play");
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    FullZoom.reset(); // must be called before closing the tab we zoomed!
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });
});

add_task(async function() {
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gTestBrowser = gBrowser.selectedBrowser;

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 1, Should not have a click-to-play notification");

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_zoom.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await promisePopupNotification("click-to-play-plugins");
});

// Enlarges the zoom level 4 times and tests that the overlay is
// visible after each enlargement.
add_task(async function() {
  for (let count = 0; count < 4; count++) {

    FullZoom.enlarge();

    // Reload the page
    await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_zoom.html");
    await promiseUpdatePluginBindings(gTestBrowser);
    await ContentTask.spawn(gTestBrowser, { count }, async function(args) {
      let doc = content.document;
      let plugin = doc.getElementById("test");
      let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
      Assert.ok(overlay && overlay.classList.contains("visible"),
        "Overlay should be visible for zoom change count " + args.count);
    });
  }
});


