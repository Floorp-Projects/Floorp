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
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  gTestBrowser.contentWindow.location = gHttpTestRoot + "plugin_both.html";
}

function pageLoad(aEvent) {
  // Due to layout being async, "PluginBindAttached" may trigger later.
  // This forces a layout flush, thus triggering it, and schedules the
  // test so it is definitely executed afterwards.
  gTestBrowser.contentDocument.getElementById('test').clientTop;
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
