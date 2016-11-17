var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);
var gTestBrowser = null;

// Test clearing plugin data using sanitize.js.
const testURL1 = gTestRoot + "browser_clearplugindata.html";
const testURL2 = gTestRoot + "browser_clearplugindata_noage.html";

var tempScope = {};
Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader)
                                           .loadSubScript("chrome://browser/content/sanitize.js", tempScope);
var Sanitizer = tempScope.Sanitizer;

const pluginHostIface = Ci.nsIPluginHost;
var pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
pluginHost.QueryInterface(pluginHostIface);

var pluginTag = getTestPlugin();
var sanitizer = null;

function stored(needles) {
  let something = pluginHost.siteHasData(this.pluginTag, null);
  if (!needles)
    return something;

  if (!something)
    return false;

  for (let i = 0; i < needles.length; ++i) {
    if (!pluginHost.siteHasData(this.pluginTag, needles[i]))
      return false;
  }
  return true;
}

add_task(function* () {
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("plugins.click_to_play");
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    if (gTestBrowser) {
      gBrowser.removeCurrentTab();
    }
    window.focus();
    gTestBrowser = null;
  });
});

add_task(function* () {
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");

  sanitizer = new Sanitizer();
  sanitizer.ignoreTimespan = false;
  sanitizer.prefDomain = "privacy.cpd.";
  let itemPrefs = gPrefService.getBranch(sanitizer.prefDomain);
  itemPrefs.setBoolPref("history", false);
  itemPrefs.setBoolPref("downloads", false);
  itemPrefs.setBoolPref("cache", false);
  itemPrefs.setBoolPref("cookies", true); // plugin data
  itemPrefs.setBoolPref("formdata", false);
  itemPrefs.setBoolPref("offlineApps", false);
  itemPrefs.setBoolPref("passwords", false);
  itemPrefs.setBoolPref("sessions", false);
  itemPrefs.setBoolPref("siteSettings", false);
});

add_task(function* () {
  // Load page to set data for the plugin.
  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;

  yield promiseTabLoadEvent(gBrowser.selectedTab, testURL1);

  yield promiseUpdatePluginBindings(gTestBrowser);

  ok(stored(["foo.com", "bar.com", "baz.com", "qux.com"]),
    "Data stored for sites");

  // Clear 20 seconds ago
  let now_uSec = Date.now() * 1000;
  sanitizer.range = [now_uSec - 20 * 1000000, now_uSec];
  yield sanitizer.sanitize();

  ok(stored(["bar.com", "qux.com"]), "Data stored for sites");
  ok(!stored(["foo.com"]), "Data cleared for foo.com");
  ok(!stored(["baz.com"]), "Data cleared for baz.com");

  // Clear everything
  sanitizer.range = null;
  yield sanitizer.sanitize();

  ok(!stored(null), "All data cleared");

  gBrowser.removeCurrentTab();
  gTestBrowser = null;
});

add_task(function* () {
  // Load page to set data for the plugin.
  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;

  yield promiseTabLoadEvent(gBrowser.selectedTab, testURL2);

  yield promiseUpdatePluginBindings(gTestBrowser);

  ok(stored(["foo.com", "bar.com", "baz.com", "qux.com"]),
    "Data stored for sites");

  // Attempt to clear 20 seconds ago. The plugin will throw
  // NS_ERROR_PLUGIN_TIME_RANGE_NOT_SUPPORTED, which should result in us
  // clearing all data regardless of age.
  let now_uSec = Date.now() * 1000;
  sanitizer.range = [now_uSec - 20 * 1000000, now_uSec];
  yield sanitizer.sanitize();

  ok(!stored(null), "All data cleared");

  gBrowser.removeCurrentTab();
  gTestBrowser = null;
});

