"use strict";

let rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

let gTestBrowser = null;

add_task(function* () {
  registerCleanupFunction(function () {
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

add_task(function* () {
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 1, Should not have a click-to-play notification");

  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_zoom.html");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  yield promisePopupNotification("click-to-play-plugins");
});

// Enlarges the zoom level 4 times and tests that the overlay is
// visible after each enlargement.
add_task(function* () {
  for (let count = 0; count < 4; count++) {

    FullZoom.enlarge();

    // Reload the page
    yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_zoom.html");
    yield promiseUpdatePluginBindings(gTestBrowser);
    let result = yield ContentTask.spawn(gTestBrowser, {}, function* () {
      let doc = content.document;
      let plugin = doc.getElementById("test");
      let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
      return overlay && overlay.classList.contains("visible");
    });
    ok(result, "Overlay should be visible for zoom change count " + count);
  }
});


