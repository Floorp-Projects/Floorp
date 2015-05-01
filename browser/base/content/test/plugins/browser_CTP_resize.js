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
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });
});

add_task(function* () {
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 1, Should not have a click-to-play notification");

  yield promiseTabLoadEvent(newTab, gTestRoot + "plugin_small.html"); // 10x10 plugin

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  yield promisePopupNotification("click-to-play-plugins");
});

// Test that the overlay is hidden for "small" plugin elements and is shown
// once they are resized to a size that can hold the overlay
add_task(function* () {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 2, Should have a click-to-play notification");

  let result = yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    return overlay && overlay.classList.contains("visible");
  });
  ok(!result, "Test 2, overlay should be hidden.");
});

add_task(function* () {
  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let plugin = content.document.getElementById("test");
    plugin.style.width = "300px";
  });

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  let result = yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    return overlay && overlay.classList.contains("visible");
  });
  ok(!result, "Test 3, overlay should be hidden.");
});


add_task(function* () {
  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let plugin = content.document.getElementById("test");
    plugin.style.height = "300px";
  });

  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    content.document.getElementById("test").clientTop;
  });

  let result = yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    return overlay && overlay.classList.contains("visible");
  });
  ok(result, "Test 4, overlay should be visible.");
});

add_task(function* () {
  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let plugin = content.document.getElementById("test");
    plugin.style.width = "10px";
    plugin.style.height = "10px";
  });

  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    content.document.getElementById("test").clientTop;
  });

  let result = yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    return overlay && overlay.classList.contains("visible");
  });
  ok(!result, "Test 5, overlay should be hidden.");
});

add_task(function* () {
  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let plugin = content.document.getElementById("test");
    plugin.style.height = "300px";
    plugin.style.width = "300px";
  });

  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    content.document.getElementById("test").clientTop;
  });

  let result = yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    return overlay && overlay.classList.contains("visible");
  });
  ok(result, "Test 6, overlay should be visible.");
});
