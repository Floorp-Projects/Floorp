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

// Test that the notification bar is getting dismissed when directly activating plugins
// via the doorhanger.

function test1() {
  let notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  waitForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") !== null,
    test2,
    "Test 1, expected a notification bar for hidden plugins");
}

function test2() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 2, Should have a click-to-play notification");
  let plugin = gTestBrowser.contentDocument.getElementById("test");
  ok(plugin, "Test 2, Found plugin in page");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
     "Test 2, Plugin should be click-to-play");

  // simulate "always allow"
  notification.reshow();
  PopupNotifications.panel.firstChild._primaryButton.click();

  let notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  waitForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") === null,
    test3,
    "Test 2, expected the notification bar for hidden plugins to get dismissed");
}

function test3() {
  let plugin = gTestBrowser.contentDocument.getElementById("test");
  ok(plugin, "Test 3, Found plugin in page");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  waitForCondition(() => objLoadingContent.activated, finishTest,
    "Test 3, Waited too long for plugin to activate");
}
