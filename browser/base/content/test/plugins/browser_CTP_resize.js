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
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });
});

add_task(async function() {
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 1, Should not have a click-to-play notification");

  await promiseTabLoadEvent(newTab, gTestRoot + "plugin_small.html"); // 10x10 plugin

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await promisePopupNotification("click-to-play-plugins");
});

// Test that the overlay is hidden for "small" plugin elements and is shown
// once they are resized to a size that can hold the overlay
add_task(async function() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 2, Should have a click-to-play notification");

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(!(overlay && overlay.classList.contains("visible")),
      "Test 2, overlay should be hidden.");
  });
});

add_task(async function() {
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let plugin = content.document.getElementById("test");
    plugin.style.width = "300px";
  });

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(!(overlay && overlay.classList.contains("visible")),
      "Test 3, overlay should be hidden.");
  });
});


add_task(async function() {
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let plugin = content.document.getElementById("test");
    plugin.style.height = "300px";
  });

  await ContentTask.spawn(gTestBrowser, {}, async function() {
    content.document.getElementById("test").clientTop;
  });

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(overlay && overlay.classList.contains("visible"),
      "Test 4, overlay should be visible.");
  });
});

add_task(async function() {
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let plugin = content.document.getElementById("test");
    plugin.style.width = "10px";
    plugin.style.height = "10px";
  });

  await ContentTask.spawn(gTestBrowser, {}, async function() {
    content.document.getElementById("test").clientTop;
  });

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(!(overlay && overlay.classList.contains("visible")),
      "Test 5, overlay should be hidden.");
  });
});

add_task(async function() {
  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let plugin = content.document.getElementById("test");
    plugin.style.height = "300px";
    plugin.style.width = "300px";
  });

  await ContentTask.spawn(gTestBrowser, {}, async function() {
    content.document.getElementById("test").clientTop;
  });

  await ContentTask.spawn(gTestBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
    Assert.ok(overlay && overlay.classList.contains("visible"),
      "Test 6, overlay should be visible.");
  });
});
