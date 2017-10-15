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
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  Services.prefs.setBoolPref("plugins.click_to_play", true);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Second Test Plug-in");

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gBrowser.selectedBrowser);

  // Tests that the overlay can be hidden for plugins using the close icon.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    let closeIcon = doc.getAnonymousElementByAttribute(plugin, "anonid", "closeIcon");
    let bounds = closeIcon.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);

    Assert.ok(!overlay.classList.contains("visible"), "overlay should be hidden.");
  });
});

// Test that the overlay cannot be interacted with after the user closes the overlay
add_task(async function() {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Second Test Plug-in");

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gBrowser.selectedBrowser);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    let closeIcon = doc.getAnonymousElementByAttribute(plugin, "anonid", "closeIcon");
    let closeIconBounds = closeIcon.getBoundingClientRect();
    let overlayBounds = overlay.getBoundingClientRect();
    let overlayLeft = (overlayBounds.left + overlayBounds.right) / 2;
    let overlayTop = (overlayBounds.left + overlayBounds.right) / 2 ;
    let closeIconLeft = (closeIconBounds.left + closeIconBounds.right) / 2;
    let closeIconTop = (closeIconBounds.top + closeIconBounds.bottom) / 2;
    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    // Simulate clicking on the close icon.
    utils.sendMouseEvent("mousedown", closeIconLeft, closeIconTop, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", closeIconLeft, closeIconTop, 0, 1, 0, false, 0, 0);

    // Simulate clicking on the overlay.
    utils.sendMouseEvent("mousedown", overlayLeft, overlayTop, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", overlayLeft, overlayTop, 0, 1, 0, false, 0, 0);

    Assert.ok(overlay.hasAttribute("dismissed") && !overlay.classList.contains("visible"),
      "Overlay should be hidden");
  });

  let notification = PopupNotifications.getNotification("click-to-play-plugins");

  ok(notification.dismissed, "No notification should be shown");
});
