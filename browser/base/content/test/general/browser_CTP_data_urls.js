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

  prepareTest(test1a, gHttpTestRoot + "plugin_data_url.html");
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

// Test that the click-to-play doorhanger still works when navigating to data URLs
function test1a() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 1a, Should have a click-to-play notification");

  let plugin = gTestBrowser.contentDocument.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 1a, Plugin should not be activated");

  gNextTest = runAfterPluginBindingAttached(test1b);
  gTestBrowser.contentDocument.getElementById("data-link-1").click();
}

function test1b() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 1b, Should have a click-to-play notification");

  let plugin = gTestBrowser.contentDocument.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 1b, Plugin should not be activated");

  // Simulate clicking the "Allow Always" button.
  popupNotification.reshow();
  PopupNotifications.panel.firstChild._primaryButton.click();

  let condition = function() objLoadingContent.activated;
  waitForCondition(condition, test1c, "Test 1b, Waited too long for plugin to activate");
}

function test1c() {
  clearAllPluginPermissions();
  prepareTest(runAfterPluginBindingAttached(test2a), gHttpTestRoot + "plugin_data_url.html");
}

// Test that the click-to-play notification doesn't break when navigating to data URLs with multiple plugins
function test2a() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 2a, Should have a click-to-play notification");
  let plugin = gTestBrowser.contentDocument.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 2a, Plugin should not be activated");

  gNextTest = runAfterPluginBindingAttached(test2b);
  gTestBrowser.contentDocument.getElementById("data-link-2").click();
}

function test2b() {
  let notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "Test 2b, Should have a click-to-play notification");

  // Simulate choosing "Allow now" for the test plugin
  notification.reshow();
  is(notification.options.pluginData.size, 2, "Test 2b, Should have two types of plugin in the notification");

  var centerAction = null;
  for (var action of notification.options.pluginData.values()) {
    if (action.pluginName == "Test") {
      centerAction = action;
      break;
    }
  }
  ok(centerAction, "Test 2b, found center action for the Test plugin");

  var centerItem = null;
  for (var item of PopupNotifications.panel.firstChild.childNodes) {
    is(item.value, "block", "Test 2b, all plugins should start out blocked");
    if (item.action == centerAction) {
      centerItem = item;
      break;
    }
  }
  ok(centerItem, "Test 2b, found center item for the Test plugin");

  // "click" the button to activate the Test plugin
  centerItem.value = "allownow";
  PopupNotifications.panel.firstChild._primaryButton.click();

  let plugin = gTestBrowser.contentDocument.getElementById("test1");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  let condition = function() objLoadingContent.activated;
  waitForCondition(condition, test2c, "Test 2b, Waited too long for plugin to activate");
}

function test2c() {
  let plugin = gTestBrowser.contentDocument.getElementById("test1");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 2c, Plugin should be activated");

  clearAllPluginPermissions();
  prepareTest(runAfterPluginBindingAttached(test3a), gHttpTestRoot + "plugin_data_url.html");
}

// Test that when navigating to a data url, the plugin permission is inherited
function test3a() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 3a, Should have a click-to-play notification");
  let plugin = gTestBrowser.contentDocument.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 3a, Plugin should not be activated");

  // Simulate clicking the "Allow Always" button.
  popupNotification.reshow();
  PopupNotifications.panel.firstChild._primaryButton.click();

  let condition = function() objLoadingContent.activated;
  waitForCondition(condition, test3b, "Test 3a, Waited too long for plugin to activate");
}

function test3b() {
  gNextTest = test3c;
  gTestBrowser.contentDocument.getElementById("data-link-1").click();
}

function test3c() {
  let plugin = gTestBrowser.contentDocument.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 3c, Plugin should be activated");

  clearAllPluginPermissions();
  prepareTest(runAfterPluginBindingAttached(test4b),
              'data:text/html,<embed id="test" style="width: 200px; height: 200px" type="application/x-test"/>');
}

// Test that the click-to-play doorhanger still works when directly navigating to data URLs
function test4a() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 4a, Should have a click-to-play notification");
  let plugin = gTestBrowser.contentDocument.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 4a, Plugin should not be activated");

  // Simulate clicking the "Allow Always" button.
  popupNotification.reshow();
  PopupNotifications.panel.firstChild._primaryButton.click();

  let condition = function() objLoadingContent.activated;
  waitForCondition(condition, test4b, "Test 4a, Waited too long for plugin to activate");
}

function test4b() {
  clearAllPluginPermissions();
  finishTest();
}
