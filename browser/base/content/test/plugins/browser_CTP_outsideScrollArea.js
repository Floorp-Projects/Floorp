var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);

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
});

// Test that the click-to-play overlay is not hidden for elements
// partially or fully outside the viewport.

add_task(function* () {
  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_outsideScrollArea.html");

  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let doc = content.document;
    let p = doc.createElement('embed');

    p.setAttribute('id', 'test');
    p.setAttribute('type', 'application/x-test');
    p.style.left = "0";
    p.style.bottom = "200px";

    doc.getElementById('container').appendChild(p);
  });

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  yield promisePopupNotification("click-to-play-plugins");

  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let plugin = content.document.getElementById("test");
    let doc = content.document;
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(overlay && overlay.classList.contains("visible"),
      "Test 2, overlay should be visible.");
  });
});

add_task(function* () {
  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_outsideScrollArea.html");

  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let doc = content.document;
    let p = doc.createElement('embed');

    p.setAttribute('id', 'test');
    p.setAttribute('type', 'application/x-test');
    p.style.left = "0";
    p.style.bottom = "-410px";

    doc.getElementById('container').appendChild(p);
  });

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  yield promisePopupNotification("click-to-play-plugins");

  yield ContentTask.spawn(gTestBrowser, null, function* () {
    let plugin = content.document.getElementById("test");
    let doc = content.document;
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(overlay && overlay.classList.contains("visible"),
      "Test 3, overlay should be visible.");
  });
});

add_task(function* () {
  yield promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_outsideScrollArea.html");

  yield ContentTask.spawn(gTestBrowser, {}, function* () {
    let doc = content.document;
    let p = doc.createElement('embed');

    p.setAttribute('id', 'test');
    p.setAttribute('type', 'application/x-test');
    p.style.left = "-600px";
    p.style.bottom = "0";

    doc.getElementById('container').appendChild(p);
  });

  // Work around for delayed PluginBindingAttached
  yield promiseUpdatePluginBindings(gTestBrowser);

  yield promisePopupNotification("click-to-play-plugins");
  yield ContentTask.spawn(gTestBrowser, null, function* () {
    let plugin = content.document.getElementById("test");
    let doc = content.document;
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(!(overlay && overlay.classList.contains("visible")),
      "Test 4, overlay should be hidden.");
  });
});
