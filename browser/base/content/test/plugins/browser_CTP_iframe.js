let rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir;
const gHttpTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

let gTestBrowser = null;
let gNextTest = null;

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
  });
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  prepareTest(delayTest(runAfterPluginBindingAttached(test1)), gHttpTestRoot + "plugin_iframe.html");
}

function finishTest() {
  clearAllPluginPermissions();
  gTestBrowser.removeEventListener("load", pageLoad, true);
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

function pageLoad() {
  gNextTest();
}

function prepareTest(nextTest, url) {
  gNextTest = nextTest;
  gTestBrowser.contentWindow.location = url;
}

// Delay executing a test for one load event to wait for frame loads.
function delayTest(nextTest) {
  return () => {
    gNextTest = nextTest;
  }
}

// Due to layout being async, "PluginBindAttached" may trigger later.
// This wraps a function to force a layout flush, thus triggering it,
// and schedules the function execution so they're definitely executed
// afterwards.
function runAfterPluginBindingAttached(func) {
  return () => {
    let frame = gTestBrowser.contentDocument.getElementById("frame");
    let doc = frame.contentDocument;
    let elems = doc.getElementsByTagName('embed');
    if (elems.length < 1) {
      elems = doc.getElementsByTagName('object');
    }
    elems[0].clientTop;
    executeSoon(func);
  };
}

// Tests that the overlays are visible and actionable if the plugin is in an iframe.
function test1() {
  let frame = gTestBrowser.contentDocument.getElementById("frame");
  let doc = frame.contentDocument;
  let plugin = doc.getElementById("test");
  ok(plugin, "Test 1, Found plugin in page");

  let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
  ok(overlay.classList.contains("visible"), "Test 1, Plugin overlay should exist, not be hidden");
  let closeIcon = doc.getAnonymousElementByAttribute(plugin, "anonid", "closeIcon")

  EventUtils.synthesizeMouseAtCenter(closeIcon, {}, frame.contentWindow);
  let condition = () => !overlay.classList.contains("visible");
  waitForCondition(condition, test2, "Test 1, Waited too long for the overlay to become invisible.");
}

function test2() {
  prepareTest(delayTest(runAfterPluginBindingAttached(test3)), gHttpTestRoot + "plugin_iframe.html");
}

function test3() {
  let frame = gTestBrowser.contentDocument.getElementById("frame");
  let doc = frame.contentDocument;
  let plugin = doc.getElementById("test");
  ok(plugin, "Test 3, Found plugin in page");

  let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
  ok(overlay.classList.contains("visible"), "Test 3, Plugin overlay should exist, not be hidden");

  EventUtils.synthesizeMouseAtCenter(plugin, {}, frame.contentWindow);
  let condition = () => PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  waitForCondition(condition, finishTest, "Test 3, Waited too long for the doorhanger to pop up.");
}
