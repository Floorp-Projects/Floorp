var gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("plugins.click_to_play");
    gTestBrowser.removeEventListener("load", pageLoad, true);
  });

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  gTestBrowser.contentWindow.location = gHttpTestRoot + "plugin_both.html";
}

function pageLoad(aEvent) {
  // The plugin events are async dispatched and can come after the load event
  // This just allows the events to fire before we then go on to test the states
  executeSoon(actualTest);
}

function actualTest() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "should have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  ok(plugin, "should have known plugin in page");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  is(objLoadingContent.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY, "plugin fallback type should be PLUGIN_CLICK_TO_PLAY");
  ok(!objLoadingContent.activated, "plugin should not be activated");

  var unknown = gTestBrowser.contentDocument.getElementById("unknown");
  ok(unknown, "should have unknown plugin in page");

  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}
