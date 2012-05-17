var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir;

var gTestBrowser = null;
var gNextTest = null;

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() { Services.prefs.clearUserPref("plugins.click_to_play"); });
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  prepareTest(test1a, gTestRoot + "plugin_bug743421.html");
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

// Tests that navigation within the page and the window.history API doesn't break click-to-play state.
function test1a() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 1a, Should not have a click-to-play notification");
  var plugin = gTestBrowser.contentWindow.addPlugin();

  setTimeout(test1b, 500);
}

function test1b() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 1b, Should have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[0];
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 1b, Plugin should not be activated");

  popupNotification.mainAction.callback();
  setTimeout(test1c, 500);
}

function test1c() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 1c, Should not have a click-to-play notification");
  var plugin = gTestBrowser.contentWindow.addPlugin();

  setTimeout(test1d, 500);
}

function test1d() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 1d, Should not have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[1];
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 1d, Plugin should be activated");

  gNextTest = test1e;
  gTestBrowser.contentWindow.addEventListener("hashchange", test1e, false);
  gTestBrowser.contentWindow.location += "#anchorNavigation";
}

function test1e() {
  gTestBrowser.contentWindow.removeEventListener("hashchange", test1e, false);
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 1e, Should not have a click-to-play notification");
  var plugin = gTestBrowser.contentWindow.addPlugin();

  setTimeout(test1f, 500);
}

function test1f() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 1f, Should not have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[2];
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 1f, Plugin should be activated");

  gTestBrowser.contentWindow.history.replaceState({}, "", "replacedState");
  gTestBrowser.contentWindow.addPlugin();
  setTimeout(test1g, 500);
}

function test1g() {
  var popupNotification2 = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification2, "Test 1g, Should not have a click-to-play notification after replaceState");
  var plugin = gTestBrowser.contentDocument.getElementsByTagName("embed")[3];
  var objLoadingContent2 = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent2.activated, "Test 1g, Plugin should be activated");
  finishTest();
}
