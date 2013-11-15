var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir;
const gHttpTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

var gTestBrowser = null;
var gNextTest = null;
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);
var gRunNextTestAfterPluginRemoved = false;

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    gTestBrowser.removeEventListener("PluginRemoved", handlePluginRemoved, true, true);
  });
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  gTestBrowser.addEventListener("PluginRemoved", handlePluginRemoved, true, true);

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  prepareTest(runAfterPluginBindingAttached(test1), gHttpTestRoot + "plugin_two_types.html");
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

function handlePluginRemoved() {
  if (gRunNextTestAfterPluginRemoved) {
    executeSoon(gNextTest);
    gRunNextTestAfterPluginRemoved = false;
  }
}

function runAfterPluginRemoved(func) {
  gNextTest = func;
  gRunNextTestAfterPluginRemoved = true;
}

// Test that the click-to-play notification is not shown for non-plugin object elements

function test1() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 1, Should have a click-to-play notification");

  let plugin = gTestBrowser.contentDocument.getElementById("secondtestA");
  plugin.parentNode.removeChild(plugin);
  plugin = gTestBrowser.contentDocument.getElementById("secondtestB");
  plugin.parentNode.removeChild(plugin);

  let image = gTestBrowser.contentDocument.createElement("object");
  image.type = "image/png";
  image.data = "moz.png";
  gTestBrowser.contentDocument.body.appendChild(image);

  runAfterPluginRemoved(test2);
}

function test2() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 2, Should have a click-to-play notification");

  let plugin = gTestBrowser.contentDocument.getElementById("test");
  plugin.parentNode.removeChild(plugin);

  executeSoon(test3);
}

function test3() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 3, Should still have a click-to-play notification");

  finishTest();
}
