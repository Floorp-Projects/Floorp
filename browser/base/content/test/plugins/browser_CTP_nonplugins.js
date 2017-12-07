var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);

add_task(async function() {
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    Services.prefs.clearUserPref("plugins.click_to_play");
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    gBrowser.removeCurrentTab();
    window.focus();
  });
});

add_task(async function() {
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_DISABLED, "Test Plug-in");

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_two_types.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gBrowser.selectedBrowser);

  // Test that the click-to-play notification is not shown for non-plugin object elements
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser);
  ok(popupNotification, "Test 1, Should have a click-to-play notification");

  let pluginRemovedPromise = waitForEvent(gBrowser.selectedBrowser, "PluginRemoved", null, true, true);
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    let plugin = content.document.getElementById("secondtestA");
    plugin.remove();
    plugin = content.document.getElementById("secondtestB");
    plugin.remove();

    let image = content.document.createElement("object");
    image.type = "image/png";
    image.data = "moz.png";
    content.document.body.appendChild(image);
  });
  await pluginRemovedPromise;

  popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser);
  ok(popupNotification, "Test 2, Should have a click-to-play notification");

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    let plugin = content.document.getElementById("test");
    plugin.remove();
  });

  popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gBrowser.selectedBrowser);
  ok(popupNotification, "Test 3, Should still have a click-to-play notification");
});
