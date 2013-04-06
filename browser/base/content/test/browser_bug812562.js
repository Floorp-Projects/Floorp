var gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;
var gNextTest = null;

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("plugins.click_to_play");
  });

  Services.prefs.setBoolPref("plugins.click_to_play", false);
  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  setAndUpdateBlocklist(gHttpTestRoot + "blockPluginVulnerableUpdatable.xml",
  function() {
    prepareTest(testPart1, gHttpTestRoot + "plugin_test.html");
  });
}

function finishTest() {
  gTestBrowser.removeEventListener("load", pageLoad, true);
  gBrowser.removeCurrentTab();
  window.focus();
  setAndUpdateBlocklist(gHttpTestRoot + "blockNoPlugins.xml",
  function() {
    resetBlocklist();
    finish();
  });
}

function pageLoad(aEvent) {
  // The plugin events are async dispatched and can come after the load event
  // This just allows the events to fire before we then go on to test the states
  if (gNextTest != null)
    executeSoon(gNextTest);
}

function prepareTest(nextTest, url) {
  gNextTest = nextTest;
  gTestBrowser.contentWindow.location = url;
}

// Tests that the going back will reshow the notification for click-to-play
// blocklisted plugins (part 1/4)
function testPart1() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "test part 1: Should have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE, "test part 1: plugin fallback type should be PLUGIN_VULNERABLE_UPDATABLE");
  ok(!objLoadingContent.activated, "test part 1: plugin should not be activated");

  prepareTest(testPart2, "about:blank");
}

function testPart2() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "test part 2: Should not have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  ok(!plugin, "test part 2: Should not have a plugin in this page");

  Services.obs.addObserver(testPart3, "PopupNotifications-updateNotShowing", false);
  gTestBrowser.contentWindow.history.back();
}

function testPart3() {
  Services.obs.removeObserver(testPart3, "PopupNotifications-updateNotShowing");
  var condition = function() PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  waitForCondition(condition, testPart4, "test part 3: waited too long for click-to-play-plugin notification");
}

function testPart4() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "test part 4: Should have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE, "test part 4: plugin fallback type should be PLUGIN_VULNERABLE_UPDATABLE");
  ok(!objLoadingContent.activated, "test part 4: plugin should not be activated");

  finishTest();
}
