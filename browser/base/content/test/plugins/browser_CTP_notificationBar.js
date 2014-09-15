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
  info("Test 1 - expecting a notification bar for hidden plugins.");
  waitForNotificationPopup("click-to-play-plugins", gTestBrowser, () => {
    waitForNotificationBar("plugin-hidden", gTestBrowser, () => {
      // Don't use setTestPluginEnabledState here because we already saved the
      // prior value
      getTestPlugin().enabledState = Ci.nsIPluginTag.STATE_ENABLED;
      prepareTest(test2, gTestRoot + "plugin_small.html");
    });
  });
}

function test2() {
  info("Test 2 - expecting no plugin notification bar on visible plugins.");
  waitForNotificationPopup("click-to-play-plugins", gTestBrowser, () => {
    let notificationBox = gBrowser.getNotificationBox(gTestBrowser);

    waitForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") === null,
      () => {
        getTestPlugin().enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
        prepareTest(test3, gTestRoot + "plugin_overlayed.html");
      },
      "expected to not have a plugin notification bar"
    );
  });
}

function test3() {
  info("Test 3 - expecting a plugin notification bar when plugins are overlaid");
  waitForNotificationPopup("click-to-play-plugins", gTestBrowser, () => {
    waitForNotificationBar("plugin-hidden", gTestBrowser, test3b);
  });
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
  info("Test 4 - expecting a plugin notification bar when plugins are overlaid offscreen")
  waitForNotificationPopup("click-to-play-plugins", gTestBrowser, () => {
    waitForNotificationBar("plugin-hidden", gTestBrowser, test4b);
  });
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

function test5() {
  let notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  waitForCondition(() => notificationBox.getNotificationWithValue("plugin-hidden") !== null,
    test6,
    "Test 5, expected a notification bar for hidden plugins");
}

// Test that the notification bar is getting dismissed when directly activating plugins
// via the doorhanger.

function test6() {
  info("Test 6 - expecting the doorhanger to be dismissed when directly activating plugins.");
  waitForNotificationPopup("click-to-play-plugins", gTestBrowser, (notification) => {
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
  });
}

function test7() {
  let plugin = gTestBrowser.contentDocument.getElementById("test");
  ok(plugin, "Test 7, Found plugin in page");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  waitForCondition(() => objLoadingContent.activated, finishTest,
    "Test 7, Waited too long for plugin to activate");
}
