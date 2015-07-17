let rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
let gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);

add_task(function* () {
  registerCleanupFunction(function () {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    Services.prefs.clearUserPref("plugins.click_to_play");
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    gBrowser.removeCurrentTab();
    window.focus();
  });
});

add_task(function* () {
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  gBrowser.selectedTab = gBrowser.addTab();

  Services.prefs.setBoolPref("plugins.click_to_play", true);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Second Test Plug-in");

  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gBrowser.selectedBrowser);

  // Tests that the overlay can be hidden for plugins using the close icon.
  let overlayIsVisible = yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    let closeIcon = doc.getAnonymousElementByAttribute(plugin, "anonid", "closeIcon")
    let bounds = closeIcon.getBoundingClientRect();
    let left = (bounds.left + bounds.right) / 2;
    let top = (bounds.top + bounds.bottom) / 2;
    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);

    return overlay.classList.contains("visible");
  });

  ok(!overlayIsVisible, "overlay should be hidden.");
});

// Test that the overlay cannot be interacted with after the user closes the overlay
add_task(function* () {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Second Test Plug-in");

  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gBrowser.selectedBrowser);

  let overlayHidden = yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    let closeIcon = doc.getAnonymousElementByAttribute(plugin, "anonid", "closeIcon")
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

    return overlay.hasAttribute("dismissed") && !overlay.classList.contains("visible");
  });

  let notification = PopupNotifications.getNotification("click-to-play-plugins");

  ok(notification.dismissed, "No notification should be shown");
  ok(overlayHidden, "Overlay should be hidden");
});
