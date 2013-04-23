var gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("plugins.click_to_play");
    var plugin = getTestPlugin();
    plugin.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
    gTestBrowser.removeEventListener("load", pageLoad, true);
  });
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  var plugin = getTestPlugin();
  plugin.enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;

  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  gTestBrowser.contentWindow.location = gHttpTestRoot + "plugin_bug818009.html";
}

function pageLoad() {
  // The plugin events are async dispatched and can come after the load event
  // This just allows the events to fire before we then go on to test the states
  executeSoon(actualTest);
}

function actualTest() {
  var notification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(notification, "should have a click-to-play notification");
  is(notification.options.centerActions.length, 1, "should have only one type of plugin in the notification");
  is(notification.options.centerActions[0].message, "Test", "the one type of plugin should be the 'Test' plugin");

  var doc = gTestBrowser.contentDocument;
  var inner = doc.getElementById("inner");
  ok(inner, "should have 'inner' plugin");
  var innerObjLC = inner.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!innerObjLC.activated, "inner plugin shouldn't be activated");
  is(innerObjLC.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY, "inner plugin fallback type should be PLUGIN_CLICK_TO_PLAY");
  var outer = doc.getElementById("outer");
  ok(outer, "should have 'outer' plugin");
  var outerObjLC = outer.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!outerObjLC.activated, "outer plugin shouldn't be activated");
  is(outerObjLC.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_ALTERNATE, "outer plugin fallback type should be PLUGIN_ALTERNATE");

  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}
