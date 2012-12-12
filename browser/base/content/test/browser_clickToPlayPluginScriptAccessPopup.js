const gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
const EXPECTED_PLUGINSCRIPTED_EVENT_COUNT = 6;

var gTestBrowser = null;
var gNextTestList = [];
var gNextTest = null;
var gPluginScriptedFired = false;
var gPluginScriptedFiredCount = 0;

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("plugins.click_to_play");
    gTestBrowser.removeEventListener("load", pageLoad, true);
    gTestBrowser.removeEventListener("PluginScripted", pluginScripted, true);
  });
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  gTestBrowser.addEventListener("PluginScripted", pluginScripted, true);

  // This list is iterated in reverse order, since it uses Array.pop to get the next test.
  gNextTestList = [
    // Doesn't show a popup since not the first instance of a small plugin
    { func: testExpectNoPopupPart1,
      url: gHttpTestRoot + "plugin_test_scriptedPopup1.html" },
    // Doesn't show a popup since not the first instance of a small plugin
    { func: testExpectNoPopupPart1,
      url: gHttpTestRoot + "plugin_test_scriptedPopup2.html" },
    // Shows a popup since it is the first instance of a small plugin
    { func: testExpectPopupPart1,
      url: gHttpTestRoot + "plugin_test_scriptedPopup3.html" },
    { func: testExpectNoPopupPart1,
      url: gHttpTestRoot + "plugin_test_scriptedNoPopup1.html" },
    { func: testExpectNoPopupPart1,
      url: gHttpTestRoot + "plugin_test_scriptedNoPopup2.html" },
    { func: testExpectNoPopupPart1,
      url: gHttpTestRoot + "plugin_test_scriptedNoPopup3.html" }
  ];

  prepareTest(testNoEventFired, gHttpTestRoot + "plugin_test_noScriptNoPopup.html");
}

function getCurrentTestLocation() {
  var loc = gTestBrowser.contentWindow.location.toString();
  return loc.replace(gHttpTestRoot, "");
}

function runNextTest() {
  var nextTest = gNextTestList.pop();
  if (nextTest) {
    gPluginScriptedFired = false;
    prepareTest(nextTest.func, nextTest.url);
  }
  else {
    finishTest();
  }
}

function finishTest() {
  is(gPluginScriptedFiredCount, EXPECTED_PLUGINSCRIPTED_EVENT_COUNT, "PluginScripted event count is correct");
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

function pluginScripted() {
  gPluginScriptedFired = true;
  gPluginScriptedFiredCount++;
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

function testNoEventFired() {
  var notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "should have a click-to-play notification (" + getCurrentTestLocation() + ")");
  ok(notification.dismissed, "notification should not be showing (" + getCurrentTestLocation() + ")");
  ok(!gPluginScriptedFired, "PluginScripted should not have fired (" + getCurrentTestLocation() + ")");

  runNextTest();
}

function testExpectNoPopupPart1() {
  var condition = function() gPluginScriptedFired;
  waitForCondition(condition, testExpectNoPopupPart2, "waited too long for PluginScripted event (" + getCurrentTestLocation() + ")");
}

function testExpectNoPopupPart2() {
  var condition = function() gTestBrowser._pluginScriptedState == PLUGIN_SCRIPTED_STATE_DONE;
  waitForCondition(condition, testExpectNoPopupPart3, "waited too long for PluginScripted event handling (" + getCurrentTestLocation() + ")");
}

function testExpectNoPopupPart3() {
  var notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "should have a click-to-play notification (" + getCurrentTestLocation() + ")");
  ok(notification.dismissed, "notification should not be showing (" + getCurrentTestLocation() + ")");

  runNextTest();
}

function testExpectPopupPart1() {
  var notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "should have a click-to-play notification (" + getCurrentTestLocation() + ")");

  var condition = function() !notification.dismissed;
  waitForCondition(condition, testExpectPopupPart2, "waited too long for popup notification to show (" + getCurrentTestLocation() + ")");
}

function testExpectPopupPart2() {
  var notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!notification.dismissed, "notification should be showing (" + getCurrentTestLocation() + ")");

  runNextTest();
}
