var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir;
const gHttpTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

var gTestBrowser = null;
var gNextTest = null;

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
  });
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  prepareTest(runAfterPluginBindingAttached(test1), gHttpTestRoot + "plugin_small.html");
}

function finishTest() {
  gTestBrowser.removeEventListener("load", pageLoad, true);
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

function pageLoad() {
  // The plugin events are async dispatched and can come after the load event
  // This just allows the events to fire before we then go on to test the states
  executeSoon(gNextTest);
}

function prepareTest(nextTest, url) {
  gNextTest = nextTest;
  gTestBrowser.contentWindow.location = url;
}

// Due to layout being async, "PluginBindAttached" may trigger later.
// This wraps a function to force a layout flush, thus triggering it,
// and schedules the function execution so they're definitely executed
// afterwards.
function runAfterPluginBindingAttached(func) {
  return function() {
    let doc = gTestBrowser.contentDocument;
    let elems = doc.getElementsByTagName('embed');
    if (elems.length < 1) {
      elems = doc.getElementsByTagName('object');
    }
    elems[0].clientTop;
    executeSoon(func);
  };
}

// Tests for the notification bar for hidden plugins.

function test1() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins");
  ok(notification, "Test 1: There should be a plugin notification");

  let notificationBox = gBrowser.getNotificationBox(gTestBrowser);

  waitForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") !== null,
    () => {
      // Don't use setTestPluginEnabledState here because we already saved the
      // prior value
      getTestPlugin().enabledState = Ci.nsIPluginTag.STATE_ENABLED;
      prepareTest(test2, gTestRoot + "plugin_small.html");
    },
    "Test 1, expected to have a plugin notification bar");
}

function test2() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins");
  ok(notification, "Test 2: There should be a plugin notification");

  let notificationBox = gBrowser.getNotificationBox(gTestBrowser);

  waitForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") === null,
    () => {
      getTestPlugin().enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
      prepareTest(test3, gTestRoot + "plugin_overlayed.html");
    },
    "Test 2, expected to not have a plugin notification bar");
}

function test3() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins");
  ok(notification, "Test 3: There should be a plugin notification");

  let notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  waitForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") !== null,
    test3b,
    "Test 3, expected the plugin infobar to be triggered when plugin was overlayed");
}

function test3b()
{
  let doc = gTestBrowser.contentDocument;
  let plugin = doc.getElementById("test");
  ok(plugin, "Test 3b, Found plugin in page");
  plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(plugin.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
     "Test 3b, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");
  ok(!plugin.activated, "Test 3b, Plugin should not be activated");
  let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
  ok(!overlay.classList.contains("visible"), "Test 3b, Plugin overlay should be hidden");

  prepareTest(test4, gTestRoot + "plugin_positioned.html");
}

function test4() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins");
  ok(notification, "Test 4: There should be a plugin notification");

  let notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  waitForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") !== null,
    test4b,
    "Test 4, expected the plugin infobar to be triggered when plugin was overlayed");
}

function test4b() {
  let doc = gTestBrowser.contentDocument;
  let plugin = doc.getElementById("test");
  ok(plugin, "Test 4b, Found plugin in page");
  plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(plugin.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
     "Test 4b, plugin fallback type should be PLUGIN_CLICK_TO_PLAY");
  ok(!plugin.activated, "Test 4b, Plugin should not be activated");
  let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
  ok(!overlay.classList.contains("visible"), "Test 4b, Plugin overlay should be hidden");

  prepareTest(runAfterPluginBindingAttached(test5), gHttpTestRoot + "plugin_small.html");
}

// Test that the notification bar is getting dismissed when directly activating plugins
// via the doorhanger.

function test5() {
  let notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  waitForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") !== null,
    test6,
    "Test 5, expected a notification bar for hidden plugins");
}

function test6() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 6, Should have a click-to-play notification");
  let plugin = gTestBrowser.contentDocument.getElementById("test");
  ok(plugin, "Test 6, Found plugin in page");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
     "Test 6, Plugin should be click-to-play");

  // simulate "always allow"
  notification.reshow();
  PopupNotifications.panel.firstChild._primaryButton.click();

  let notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  waitForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") === null,
    test7,
    "Test 6, expected the notification bar for hidden plugins to get dismissed");
}

function test7() {
  let plugin = gTestBrowser.contentDocument.getElementById("test");
  ok(plugin, "Test 7, Found plugin in page");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  waitForCondition(() => objLoadingContent.activated, finishTest,
    "Test 7, Waited too long for plugin to activate");
}
