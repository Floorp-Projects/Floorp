var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir;
const gHttpTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

var gTestBrowser = null;
var gNextTest = null;
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);

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
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Second Test Plug-in");

  prepareTest(test1a, gHttpTestRoot + "plugin_two_types.html");
}

function finishTest() {
  clearAllPluginPermissions();
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

// Test that the click-to-play doorhanger for multiple plugins shows the correct
// state when re-opening without reloads or navigation.

function test1a() {
  let doc = gTestBrowser.contentDocument;
  let plugin = doc.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test1a, Plugin should not be activated");

  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 1a, Should have a click-to-play notification");
  notification.reshow();

  is(notification.options.pluginData.size, 2,
     "Test 1a, Should have two types of plugin in the notification");

  let pluginItem = null;
  for (let item of PopupNotifications.panel.firstChild.childNodes) {
    is(item.value, "block", "Test 1a, all plugins should start out blocked");
    if (item.action.pluginName == "Test") {
      pluginItem = item;
    }
  }

  // Choose "Allow now" for the test plugin
  pluginItem.value = "allownow";
  PopupNotifications.panel.firstChild._primaryButton.click();

  waitForCondition(() => objLoadingContent.activated, test1b,
                   "Test 1a, Waited too long for plugin to activate");
}

function test1b() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 1b, Should have a click-to-play notification");
  notification.reshow();

  let pluginItem = null;
  for (let item of PopupNotifications.panel.firstChild.childNodes) {
    if (item.action.pluginName == "Test") {
      is(item.value, "allownow", "Test 1b, Test plugin should now be set to 'Allow now'");
    } else {
      is(item.value, "block", "Test 1b, Second Test plugin should still be blocked");
      pluginItem = item;
    }
  }

  // Choose "Allow and remember" for the Second Test plugin
  pluginItem.value = "allowalways";
  PopupNotifications.panel.firstChild._primaryButton.click();

  let doc = gTestBrowser.contentDocument;
  let plugin = doc.getElementById("secondtestA");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  waitForCondition(() => objLoadingContent.activated, test1c,
                   "Test 1b, Waited too long for plugin to activate");
}

function test1c() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 1c, Should have a click-to-play notification");
  notification.reshow();

  for (let item of PopupNotifications.panel.firstChild.childNodes) {
    if (item.action.pluginName == "Test") {
      is(item.value, "allownow", "Test 1c, Test plugin should be set to 'Allow now'");
    } else {
      is(item.value, "allowalways", "Test 1c, Second Test plugin should be set to 'Allow always'");
    }
  }

  finishTest();
}
